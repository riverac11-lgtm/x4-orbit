#include "OrbitCatalogActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "CatalogSyncActivity.h"
#include "MappedInputManager.h"
#include "OpdsBookBrowserActivity.h"
#include "activities/ActivityManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "util/OpdsOnDevice.h"
#include "util/OrbitCatalogFiles.h"

namespace fui = freeink::ui;
using OrbitCatalog::RecordView;

namespace {
enum RootRow : int { kRowSearch, kRowWaiting, kRowNew, kRowSeries, kRowAuthors, kRowLive, kRowUpdate, kRootRows };

std::string_view nameField(const RecordView& view, const bool series) {
  return series ? view[OrbitCatalog::kSeries] : view[OrbitCatalog::kAuthor];
}

uint32_t foldedHash(std::string_view text) {
  std::string folded;
  OrbitCatalog::fold(text, folded);
  return OrbitCatalog::idHash(folded);
}

// Copies a view into a fixed buffer, cut on a character boundary.
void copyField(char* dst, const size_t cap, std::string_view src) {
  size_t n = std::min(src.size(), cap - 1);
  while (n > 0 && n < src.size() && (static_cast<unsigned char>(src[n]) & 0xC0) == 0x80) --n;
  memcpy(dst, src.data(), n);
  dst[n] = '\0';
}
}  // namespace

OrbitCatalogActivity::OrbitCatalogActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           const OpdsServer& server)
    : UiListActivity("OrbitCatalog", renderer, mappedInput),
      server(server),
      catPath(OrbitCatalogFiles::catalogPath(server.url)),
      waitPath(OrbitCatalogFiles::waitingPath(server.url)),
      lineBuf(new (std::nothrow) char[OrbitCatalog::kMaxLine]),
      rowBuf(new (std::nothrow) char[OrbitCatalog::kMaxLine]) {}

void OrbitCatalogActivity::onEnter() {
  UiListActivity::onEnter();
  loadState();
  showRoot();
}

bool OrbitCatalogActivity::loadState() {
  if (!lineBuf || !rowBuf) return false;
  haveCatalog = OrbitCatalogFiles::readHeader(catPath, OrbitCatalog::kCatalogKind, header);
  newCount = 0;
  if (haveCatalog && header.gen > 1) {
    struct Ctx {
      uint16_t gen;
      int count;
    } ctx{header.gen, 0};
    OrbitCatalogFiles::forEach(
        catPath, OrbitCatalog::kCatalogKind, lineBuf.get(),
        [](void* p, uint32_t, const RecordView& v) {
          auto& c = *static_cast<Ctx*>(p);
          if (v.gen() == c.gen) ++c.count;
          return true;
        },
        &ctx);
    newCount = ctx.count;
  }
  OpdsOnDevice::scan(onDeviceSet);
  reloadWaiting();
  return true;
}

void OrbitCatalogActivity::reloadWaiting() {
  OrbitCatalogFiles::loadIds(waitPath, OrbitCatalog::kWaitingKind, lineBuf.get(), waitingSet);
  waitingCount = static_cast<int>(waitingSet.size());
}

bool OrbitCatalogActivity::isWaiting(std::string_view id) const {
  return std::binary_search(waitingSet.begin(), waitingSet.end(), OrbitCatalog::idHash(id));
}

bool OrbitCatalogActivity::onDevice(const RecordView& v) const {
  return OpdsOnDevice::contains(onDeviceSet, std::string(v[OrbitCatalog::kAuthor]),
                                std::string(v[OrbitCatalog::kTitle]));
}

const std::string& OrbitCatalogActivity::listPath() const { return kind == Kind::Waiting ? waitPath : catPath; }

bool OrbitCatalogActivity::bookAt(const uint32_t offset, RecordView& v) const {
  return OrbitCatalogFiles::readAt(listPath(), offset, lineBuf.get(), v);
}

int OrbitCatalogActivity::leadRow() const {
  if (view != View::Books) return -1;
  if (kind == Kind::Series && missingCount > 0) return 0;
  if (kind == Kind::Waiting && !rows.empty()) return 0;
  return -1;
}

void OrbitCatalogActivity::setRows(const View v, const Kind k, const int selected) {
  view = v;
  kind = k;
  const int count = listCount();
  moveSelectionTo(count > 0 ? std::clamp(selected, 0, count - 1) : 0);
  requestUpdate();
}

int OrbitCatalogActivity::listCount() const {
  switch (view) {
    case View::Root:
      return haveCatalog ? kRootRows : 2;
    case View::Books:
      return static_cast<int>(rows.size()) + (leadRow() == 0 ? 1 : 0);
    case View::Names:
      return static_cast<int>(names.size());
    case View::Actions:
      return 2;
  }
  return 0;
}

void OrbitCatalogActivity::showRoot(const int selected) {
  RenderLock lock(*this);
  rows.clear();
  rows.shrink_to_fit();
  names.clear();
  names.shrink_to_fit();
  setRows(View::Root, Kind::Search, selected);
}

void OrbitCatalogActivity::showBooks(const Kind k, const int selected) {
  RenderLock lock(*this);
  rows.clear();
  missingCount = 0;
  struct Ctx {
    OrbitCatalogActivity* self;
    Kind kind;
    std::vector<std::pair<int32_t, uint32_t>> ordered;  // series: (number, offset)
  } ctx{this, k, {}};
  const std::string& path = k == Kind::Waiting ? waitPath : catPath;
  OrbitCatalogFiles::forEach(
      path, k == Kind::Waiting ? OrbitCatalog::kWaitingKind : OrbitCatalog::kCatalogKind, lineBuf.get(),
      [](void* p, const uint32_t offset, const RecordView& v) {
        auto& c = *static_cast<Ctx*>(p);
        OrbitCatalogActivity& s = *c.self;
        bool take = false;
        switch (c.kind) {
          case Kind::Waiting:
            take = true;
            break;
          case Kind::New:
            take = s.header.gen > 1 && v.gen() == s.header.gen;
            break;
          case Kind::Search:
            take = OrbitCatalog::containsFolded(v[OrbitCatalog::kTitle], s.query) ||
                   OrbitCatalog::containsFolded(v[OrbitCatalog::kAuthor], s.query) ||
                   OrbitCatalog::containsFolded(v[OrbitCatalog::kSeries], s.query);
            break;
          case Kind::Series:
          case Kind::Author: {
            const std::string_view field = nameField(v, c.kind == Kind::Series);
            take = !field.empty() && foldedHash(field) == s.nameFilter;
            break;
          }
          default:
            break;
        }
        if (!take) return true;
        if (c.kind == Kind::Series) {
          c.ordered.emplace_back(OrbitCatalog::seriesOrderKey(v[OrbitCatalog::kSeriesIndex]), offset);
          if (!s.onDevice(v) && !s.isWaiting(v[OrbitCatalog::kId])) ++s.missingCount;
        } else {
          s.rows.push_back(offset);
        }
        return (s.rows.size() + c.ordered.size()) < kMaxRows;
      },
      &ctx);
  if (k == Kind::Series) {
    // Insertion sort, stable (equal numbers keep the catalog's order): a series is short, and
    // std::stable_sort costs ~1.5 KB of flash this firmware does not have.
    auto& o = ctx.ordered;
    for (size_t i = 1; i < o.size(); ++i) {
      const auto item = o[i];
      size_t j = i;
      for (; j > 0 && o[j - 1].first > item.first; --j) o[j] = o[j - 1];
      o[j] = item;
    }
    rows.reserve(ctx.ordered.size());
    for (const auto& entry : ctx.ordered) rows.push_back(entry.second);
  }
  setRows(View::Books, k, selected);
}

void OrbitCatalogActivity::showNames(const Kind k, const int selected) {
  RenderLock lock(*this);
  names.clear();
  struct Ctx {
    OrbitCatalogActivity* self;
    bool series;
  } ctx{this, k == Kind::SeriesNames};
  OrbitCatalogFiles::forEach(
      catPath, OrbitCatalog::kCatalogKind, lineBuf.get(),
      [](void* p, const uint32_t offset, const RecordView& v) {
        auto& c = *static_cast<Ctx*>(p);
        auto& list = c.self->names;
        const std::string_view field = nameField(v, c.series);
        if (field.empty()) return true;
        const uint32_t h = foldedHash(field);
        for (auto& row : list) {
          if (row.hash == h) {
            if (row.count < UINT16_MAX) ++row.count;
            return true;
          }
        }
        NameRow row{h, offset, 1, {}};
        OrbitCatalog::sortKey(field, row.key);
        list.push_back(row);
        return list.size() < kMaxNames;
      },
      &ctx);
  // Insertion sort (a few hundred names at most, once per visit): std::sort's ~1 KB is spared.
  for (size_t i = 1; i < names.size(); ++i) {
    const NameRow item = names[i];
    size_t j = i;
    for (; j > 0 && memcmp(names[j - 1].key, item.key, sizeof(item.key)) > 0; --j) names[j] = names[j - 1];
    names[j] = item;
  }
  setRows(View::Names, k, selected);
}

void OrbitCatalogActivity::showActions(const int bookRow, const bool wholeSet) {
  RenderLock lock(*this);
  booksSelected = nav.selected.load();
  actionRow = wholeSet ? -1 : bookRow;
  actionTitle.clear();
  actionWaiting = false;
  RecordView v;
  if (!wholeSet && bookRow >= 0 && bookRow < static_cast<int>(rows.size()) &&
      bookAt(rows[static_cast<size_t>(bookRow)], v)) {
    actionTitle.assign(v[OrbitCatalog::kTitle]);
    actionWaiting = isWaiting(v[OrbitCatalog::kId]);
  } else if (wholeSet) {
    actionTitle = nameTitle;
  }
  view = View::Actions;
  moveSelectionTo(0);
  requestUpdate();
}

const char* OrbitCatalogActivity::headerTitle() const {
  switch (view) {
    case View::Root:
      return server.name.empty() ? tr(STR_ONLINE_LIBRARY) : server.name.c_str();
    case View::Names:
      return kind == Kind::SeriesNames ? tr(STR_CATALOG_SERIES) : tr(STR_CATALOG_AUTHORS);
    case View::Actions:
      return actionTitle.c_str();
    case View::Books:
      break;
  }
  switch (kind) {
    case Kind::Series:
    case Kind::Author:
      return nameTitle.c_str();
    case Kind::Search:
      return tr(STR_SEARCH);
    default:
      return tr(STR_ONLINE_LIBRARY);
  }
}

const char* OrbitCatalogActivity::footerConfirmLabel() const {
  if (view == View::Books && kind == Kind::Waiting && nav.selected.load() != leadRow()) return tr(STR_CATALOG_DONT_GET);
  return tr(STR_SELECT);
}

void OrbitCatalogActivity::buildScreen(UiScreen& screen) {
  layoutListArea(screen);
  if (!lineBuf || !rowBuf) return;
  if (listCount() == 0) {
    screen.centeredText(tr(STR_NO_ENTRIES), screen.theme().bodyText);
    return;
  }
  fui::ListProps props = listProps(screen);
  props.labelText = {};
  props.rowProvider = &OrbitCatalogActivity::provideRow;
  props.rowProviderCtx = this;
  props.count = static_cast<uint16_t>(listCount());
  addList(screen, props, /*hasSubtitle=*/view != View::Actions);
}

void OrbitCatalogActivity::provideRow(void* ctx, const uint16_t index, fui::ListItem& item) {
  auto* self = static_cast<OrbitCatalogActivity*>(ctx);
  item.actionValue = static_cast<int16_t>(index);
  char* label = self->labelBuf;
  char* sub = self->subBuf;
  label[0] = sub[0] = '\0';
  item.label = label;

  switch (self->view) {
    case View::Root: {
      if (!self->haveCatalog) {
        item.label = index == 0 ? tr(STR_CATALOG_SAVE) : tr(STR_CATALOG_BROWSE_LIVE);
        if (index == 0) item.subtitle = tr(STR_CATALOG_SAVE_HINT);
        return;
      }
      switch (index) {
        case kRowSearch:
          item.label = tr(STR_SEARCH);
          return;
        case kRowWaiting:
          snprintf(label, sizeof(labelBuf), tr(STR_CATALOG_WAITING), self->waitingCount);
          return;
        case kRowNew:
          snprintf(label, sizeof(labelBuf), tr(STR_CATALOG_NEW), self->newCount);
          return;
        case kRowSeries:
          item.label = tr(STR_CATALOG_SERIES);
          return;
        case kRowAuthors:
          item.label = tr(STR_CATALOG_AUTHORS);
          return;
        case kRowLive:
          item.label = tr(STR_CATALOG_BROWSE_LIVE);
          return;
        default: {
          item.label = tr(STR_CATALOG_UPDATE);
          char when[32] = "-";
          const time_t t = static_cast<time_t>(self->header.updateTime);
          struct tm tm{};
          // Not strftime(): it would link ~9 KB of newlib's integer printf for this one line.
          if (t > 0 && localtime_r(&t, &tm)) {
            snprintf(when, sizeof(when), "%04d-%02d-%02d %02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                     tm.tm_hour, tm.tm_min);
          }
          snprintf(sub, sizeof(subBuf), tr(STR_CATALOG_LAST_UPDATED), when);
          item.subtitle = sub;
          return;
        }
      }
    }
    case View::Actions: {
      item.label = index == 0 ? tr(STR_CATALOG_GET_NOW)
                              : (self->actionWaiting ? tr(STR_CATALOG_DONT_GET) : tr(STR_CATALOG_GET_CHARGING));
      return;
    }
    case View::Names: {
      if (index >= self->names.size()) return;
      const NameRow& row = self->names[index];
      RecordView v;
      if (OrbitCatalogFiles::readAt(self->catPath, row.offset, self->rowBuf.get(), v)) {
        copyField(label, sizeof(labelBuf), nameField(v, self->kind == Kind::SeriesNames));
      }
      snprintf(sub, sizeof(subBuf), "%u", static_cast<unsigned>(row.count));
      item.value = sub;
      return;
    }
    case View::Books:
      break;
  }

  const int lead = self->leadRow();
  if (lead == 0 && index == 0) {
    if (self->kind == Kind::Series) {
      snprintf(label, sizeof(labelBuf), tr(STR_CATALOG_GET_MISSING), self->missingCount);
    } else {
      item.label = tr(STR_CATALOG_GET_NOW);
    }
    return;
  }
  const size_t row = index - (lead == 0 ? 1 : 0);
  if (row >= self->rows.size()) return;
  RecordView v;
  if (!OrbitCatalogFiles::readAt(self->listPath(), self->rows[row], self->rowBuf.get(), v)) return;
  copyField(label, sizeof(labelBuf), v[OrbitCatalog::kTitle]);
  // The author, and in a series list the book's number.
  copyField(sub, sizeof(subBuf), v[OrbitCatalog::kAuthor]);
  if (self->kind == Kind::Series && !v[OrbitCatalog::kSeriesIndex].empty()) {
    const size_t n = strlen(sub);
    snprintf(sub + n, sizeof(subBuf) - n, "%s#%.*s", n ? "  " : "",
             static_cast<int>(v[OrbitCatalog::kSeriesIndex].size()), v[OrbitCatalog::kSeriesIndex].data());
  }
  if (sub[0]) item.subtitle = sub;
  if (self->onDevice(v)) {
    item.value = tr(STR_OPDS_ON_DEVICE);
  } else if (self->kind != Kind::Waiting && self->isWaiting(v[OrbitCatalog::kId])) {
    item.value = tr(STR_CATALOG_WAITING_MARK);
  }
}

void OrbitCatalogActivity::startSearch() {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_SEARCH), std::string{}, 64),
      [this](const ActivityResult& result) {
        if (result.isCancelled) return;
        OrbitCatalog::fold(std::get<KeyboardResult>(result.data).text, query);
        if (!query.empty()) showBooks(Kind::Search);
      });
}

void OrbitCatalogActivity::runSync(const uint8_t mode, const std::string& onlyId) {
  startActivityForResult(std::make_unique<CatalogSyncActivity>(renderer, mappedInput, server,
                                                               static_cast<CatalogSyncActivity::Mode>(mode), onlyId),
                         [this](const ActivityResult&) {
                           // Only reached when no network was brought up (the sync screen reboots otherwise).
                           loadState();
                           showRoot();
                         });
}

void OrbitCatalogActivity::getMissing(const bool now) {
  // Every book of the series listed that is neither on the device nor waiting joins the list.
  RecordView v;
  OrbitCatalog::Book book;
  for (const uint32_t offset : rows) {
    if (!bookAt(offset, v) || onDevice(v) || isWaiting(v[OrbitCatalog::kId])) continue;
    OrbitCatalog::toBook(v, book);
    if (!OrbitCatalogFiles::addWaiting(server.url, book, lineBuf.get())) break;
  }
  reloadWaiting();
  if (now) {
    runSync(static_cast<uint8_t>(CatalogSyncActivity::Mode::Download), {});
    return;
  }
  showBooks(Kind::Series, booksSelected);
}

void OrbitCatalogActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (!lineBuf || !rowBuf) return;
  switch (view) {
    case View::Root:
      if (!haveCatalog) {
        if (index == 0) {
          runSync(static_cast<uint8_t>(CatalogSyncActivity::Mode::Build), {});
        } else {
          activityManager.replaceActivity(
              std::make_unique<OpdsBookBrowserActivity>(renderer, mappedInput, server, OpdsBackTo::Home));
        }
        return;
      }
      switch (index) {
        case kRowSearch:
          startSearch();
          return;
        case kRowWaiting:
          showBooks(Kind::Waiting);
          return;
        case kRowNew:
          showBooks(Kind::New);
          return;
        case kRowSeries:
          showNames(Kind::SeriesNames);
          return;
        case kRowAuthors:
          showNames(Kind::AuthorNames);
          return;
        case kRowLive:
          activityManager.replaceActivity(
              std::make_unique<OpdsBookBrowserActivity>(renderer, mappedInput, server, OpdsBackTo::Home));
          return;
        default:
          runSync(static_cast<uint8_t>(CatalogSyncActivity::Mode::Update), {});
          return;
      }
    case View::Names: {
      if (index < 0 || index >= static_cast<int>(names.size())) return;
      namesSelected = index;
      RecordView v;
      if (!OrbitCatalogFiles::readAt(catPath, names[static_cast<size_t>(index)].offset, lineBuf.get(), v)) return;
      const bool series = kind == Kind::SeriesNames;
      nameTitle.assign(nameField(v, series));
      nameFilter = names[static_cast<size_t>(index)].hash;
      showBooks(series ? Kind::Series : Kind::Author);
      return;
    }
    case View::Books: {
      const int lead = leadRow();
      if (lead == 0 && index == 0) {
        if (kind == Kind::Waiting) {
          runSync(static_cast<uint8_t>(CatalogSyncActivity::Mode::Download), {});
        } else {
          showActions(-1, true);
        }
        return;
      }
      const int row = index - (lead == 0 ? 1 : 0);
      if (row < 0 || row >= static_cast<int>(rows.size())) return;
      if (kind == Kind::Waiting) {
        // Confirm takes a waiting book off the list.
        RecordView v;
        if (bookAt(rows[static_cast<size_t>(row)], v)) {
          const std::string id(v[OrbitCatalog::kId]);
          OrbitCatalogFiles::removeWaiting(server.url, id, lineBuf.get());
          reloadWaiting();
        }
        showBooks(Kind::Waiting, index);
        return;
      }
      showActions(row, false);
      return;
    }
    case View::Actions: {
      if (actionRow < 0) {
        getMissing(index == 0);
        return;
      }
      RecordView v;
      if (actionRow >= static_cast<int>(rows.size()) || !bookAt(rows[static_cast<size_t>(actionRow)], v)) return;
      OrbitCatalog::Book book;
      OrbitCatalog::toBook(v, book);
      if (index == 1 && isWaiting(book.id)) {
        OrbitCatalogFiles::removeWaiting(server.url, book.id, lineBuf.get());
      } else if (!OrbitCatalogFiles::addWaiting(server.url, book, lineBuf.get())) {
        LOG_ERR("OCAT", "Waiting list full or card error");
      } else if (index == 0) {
        runSync(static_cast<uint8_t>(CatalogSyncActivity::Mode::Download), book.id);
        return;
      }
      reloadWaiting();
      showBooks(kind, booksSelected);
      return;
    }
  }
}

void OrbitCatalogActivity::onBackButton() {
  switch (view) {
    case View::Root:
      onGoHome();
      return;
    case View::Actions:
      showBooks(kind, booksSelected);
      return;
    case View::Names:
      showRoot(kind == Kind::SeriesNames ? kRowSeries : kRowAuthors);
      return;
    case View::Books:
      if (kind == Kind::Series || kind == Kind::Author) {
        showNames(kind == Kind::Series ? Kind::SeriesNames : Kind::AuthorNames, namesSelected);
        return;
      }
      showRoot(kind == Kind::Waiting ? kRowWaiting : kind == Kind::New ? kRowNew : kRowSearch);
      return;
  }
}
