// For license of this file, see <project-root-folder>/LICENSE.md.

#include "services/standard/standardfeedsimportexportmodel.h"

#include "3rd-party/boolinq/boolinq.h"
#include "definitions/definitions.h"
#include "exceptions/applicationexception.h"
#include "miscellaneous/application.h"
#include "miscellaneous/iconfactory.h"
#include "services/standard/definitions.h"
#include "services/standard/standardcategory.h"
#include "services/standard/standardfeed.h"
#include "services/standard/standardserviceroot.h"

#include <QDomAttr>
#include <QDomDocument>
#include <QDomElement>
#include <QLocale>
#include <QStack>
#include <QtConcurrent/QtConcurrentMap>

FeedsImportExportModel::FeedsImportExportModel(QObject* parent)
  : AccountCheckSortedModel(parent), m_mode(Mode::Import) {

  connect(&m_watcherLookup, &QFutureWatcher<bool>::progressValueChanged, this, [=](int prog) {
    emit parsingProgress(prog, m_lookup.size());
  });

  connect(&m_watcherLookup, &QFutureWatcher<bool>::finished, this, [=]() {
    auto res = m_watcherLookup.future().results();
    int number_error = boolinq::from(res).count([](bool rs) {
      return !rs;
    });

    emit layoutChanged();
    emit parsingFinished(number_error, m_lookup.size() - number_error);

    // Done, remove lookups.
    m_lookup.clear();
  });
}

FeedsImportExportModel::~FeedsImportExportModel() {
  if (m_watcherLookup.isRunning()) {
    m_watcherLookup.cancel();
    m_watcherLookup.waitForFinished();
  }

  if (sourceModel() != nullptr && sourceModel()->rootItem() != nullptr && m_mode == Mode::Import) {
    // Delete all model items, but only if we are in import mode. Export mode shares
    // root item with main feed model, thus cannot be deleted from memory now.
    delete sourceModel()->rootItem();
  }
}

bool FeedsImportExportModel::exportToOMPL20(QByteArray& result, bool export_icons) {
  QDomDocument opml_document;
  QDomProcessingInstruction xml_declaration =
    opml_document.createProcessingInstruction(QSL("xml"), QSL("version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\""));

  opml_document.appendChild(xml_declaration);

  // Added OPML 2.0 metadata.
  opml_document.appendChild(opml_document.createElement(QSL("opml")));
  opml_document.documentElement().setAttribute(QSL("version"), QSL("2.0"));
  opml_document.documentElement().setAttribute(QSL("xmlns:rssguard"), QSL(APP_URL));
  QDomElement elem_opml_head = opml_document.createElement(QSL("head"));
  QDomElement elem_opml_title = opml_document.createElement(QSL("title"));
  QDomText text_opml_title = opml_document.createTextNode(QSL(APP_NAME));

  elem_opml_title.appendChild(text_opml_title);
  elem_opml_head.appendChild(elem_opml_title);
  QDomElement elem_opml_created = opml_document.createElement(QSL("dateCreated"));
  QDomText text_opml_created = opml_document.createTextNode(QLocale::c().toString(QDateTime::currentDateTimeUtc(),
                                                                                  QSL("ddd, dd MMM yyyy hh:mm:ss")) +
                                                            QSL(" GMT"));

  elem_opml_created.appendChild(text_opml_created);
  elem_opml_head.appendChild(elem_opml_created);
  opml_document.documentElement().appendChild(elem_opml_head);
  QDomElement elem_opml_body = opml_document.createElement(QSL("body"));
  QStack<RootItem*> items_to_process;

  items_to_process.push(sourceModel()->rootItem());
  QStack<QDomElement> elements_to_use;

  elements_to_use.push(elem_opml_body);

  // Process all unprocessed nodes.
  while (!items_to_process.isEmpty()) {
    QDomElement active_element = elements_to_use.pop();
    RootItem* active_item = items_to_process.pop();
    auto chi = active_item->childItems();

    for (RootItem* child_item : qAsConst(chi)) {
      if (!sourceModel()->isItemChecked(child_item)) {
        continue;
      }

      switch (child_item->kind()) {
        case RootItem::Kind::Category: {
          QDomElement outline_category = opml_document.createElement(QSL("outline"));

          outline_category.setAttribute(QSL("text"), child_item->title());
          outline_category.setAttribute(QSL("description"), child_item->description());

          if (export_icons && !child_item->icon().isNull()) {
            outline_category.setAttribute(QSL("rssguard:icon"),
                                          QString(qApp->icons()->toByteArray(child_item->icon())));
          }

          active_element.appendChild(outline_category);
          items_to_process.push(child_item);
          elements_to_use.push(outline_category);
          break;
        }

        case RootItem::Kind::Feed: {
          auto* child_feed = dynamic_cast<StandardFeed*>(child_item);
          QDomElement outline_feed = opml_document.createElement("outline");

          outline_feed.setAttribute(QSL("type"), QSL("rss"));
          outline_feed.setAttribute(QSL("text"), child_feed->title());
          outline_feed.setAttribute(QSL("xmlUrl"), child_feed->source());
          outline_feed.setAttribute(QSL("description"), child_feed->description());
          outline_feed.setAttribute(QSL("encoding"), child_feed->encoding());
          outline_feed.setAttribute(QSL("title"), child_feed->title());

          outline_feed.setAttribute(QSL("rssguard:xmlUrlType"), QString::number(int(child_feed->sourceType())));
          outline_feed.setAttribute(QSL("rssguard:postProcess"), child_feed->postProcessScript());

          if (export_icons && !child_feed->icon().isNull()) {
            outline_feed.setAttribute(QSL("rssguard:icon"), QString(qApp->icons()->toByteArray(child_feed->icon())));
          }

          switch (child_feed->type()) {
            case StandardFeed::Type::Rss0X:
            case StandardFeed::Type::Rss2X:
              outline_feed.setAttribute(QSL("version"), QSL("RSS"));
              break;

            case StandardFeed::Type::Rdf:
              outline_feed.setAttribute(QSL("version"), QSL("RSS1"));
              break;

            case StandardFeed::Type::Atom10:
              outline_feed.setAttribute(QSL("version"), QSL("ATOM"));
              break;

            case StandardFeed::Type::Json:
              outline_feed.setAttribute(QSL("version"), QSL("JSON"));
              break;

            default:
              break;
          }

          active_element.appendChild(outline_feed);
          break;
        }

        default:
          break;
      }
    }
  }

  opml_document.documentElement().appendChild(elem_opml_body);
  result = opml_document.toByteArray(2);
  return true;
}

bool FeedsImportExportModel::produceFeed(const FeedLookup& feed_lookup) {
  StandardFeed* new_feed = nullptr;

  try {
    if (feed_lookup.fetch_metadata_online) {
      new_feed = StandardFeed::guessFeed(StandardFeed::SourceType::Url,
                                         feed_lookup.url,
                                         feed_lookup.post_process_script,
                                         {},
                                         {},
                                         feed_lookup.custom_proxy);

      new_feed->setSource(feed_lookup.url);
      new_feed->setPostProcessScript(feed_lookup.post_process_script);
    }
    else {
      new_feed = new StandardFeed(feed_lookup.parent);

      if (feed_lookup.opml_element.isNull()) {
        new_feed->setSource(feed_lookup.url);
        new_feed->setTitle(feed_lookup.url);
        new_feed->setIcon(qApp->icons()->fromTheme(QSL("application-rss+xml")));
        new_feed->setEncoding(QSL(DEFAULT_FEED_ENCODING));
        new_feed->setPostProcessScript(feed_lookup.post_process_script);
      }
      else {
        QString feed_title = feed_lookup.opml_element.attribute(QSL("text"));
        QString feed_encoding = feed_lookup.opml_element.attribute(QSL("encoding"), QSL(DEFAULT_FEED_ENCODING));
        QString feed_type = feed_lookup.opml_element.attribute(QSL("version"), QSL(DEFAULT_FEED_TYPE)).toUpper();
        QString feed_description = feed_lookup.opml_element.attribute(QSL("description"));
        QIcon feed_icon =
          qApp->icons()->fromByteArray(feed_lookup.opml_element.attribute(QSL("rssguard:icon")).toLocal8Bit());
        StandardFeed::SourceType source_type =
          StandardFeed::SourceType(feed_lookup.opml_element.attribute(QSL("rssguard:xmlUrlType")).toInt());
        QString post_process = feed_lookup.opml_element.attribute(QSL("rssguard:postProcess"));

        new_feed->setTitle(feed_title);
        new_feed->setDescription(feed_description);
        new_feed->setEncoding(feed_encoding);
        new_feed->setSource(feed_lookup.url);
        new_feed->setSourceType(source_type);
        new_feed->setPostProcessScript(feed_lookup.post_process_script.isEmpty() ? post_process
                                                                                 : feed_lookup.post_process_script);

        if (!feed_icon.isNull()) {
          new_feed->setIcon(feed_icon);
        }

        if (feed_type == QL1S("RSS1")) {
          new_feed->setType(StandardFeed::Type::Rdf);
        }
        else if (feed_type == QL1S("JSON")) {
          new_feed->setType(StandardFeed::Type::Json);
        }
        else if (feed_type == QL1S("ATOM")) {
          new_feed->setType(StandardFeed::Type::Atom10);
        }
        else {
          new_feed->setType(StandardFeed::Type::Rss2X);
        }
      }
    }

    QMutexLocker mtx(&m_mtxLookup);
    feed_lookup.parent->appendChild(new_feed);

    return true;
  }
  catch (const ApplicationException& ex) {
    qCriticalNN << LOGSEC_CORE << "Cannot fetch medatada for feed:" << QUOTE_W_SPACE(feed_lookup.url)
                << "with error:" << QUOTE_W_SPACE_DOT(ex.message());

    if (new_feed != nullptr) {
      new_feed->deleteLater();
    }

    return false;
  }
}

void FeedsImportExportModel::importAsOPML20(const QByteArray& data,
                                            bool fetch_metadata_online,
                                            const QString& post_process_script) {
  emit parsingStarted();
  emit layoutAboutToBeChanged();
  setRootItem(nullptr);
  emit layoutChanged();

  QDomDocument opml_document;

  if (!opml_document.setContent(data)) {
    emit parsingFinished(0, 0);
  }

  if (opml_document.documentElement().isNull() || opml_document.documentElement().tagName() != QSL("opml") ||
      opml_document.documentElement().elementsByTagName(QSL("body")).size() != 1) {
    // This really is not an OPML file.
    emit parsingFinished(0, 0);
  }

  int completed = 0, total = 0;
  auto* root_item = new StandardServiceRoot();
  QStack<RootItem*> model_items;
  QNetworkProxy custom_proxy;

  if (sourceModel()->rootItem() != nullptr && sourceModel()->rootItem()->getParentServiceRoot() != nullptr) {
    custom_proxy = sourceModel()->rootItem()->getParentServiceRoot()->networkProxy();
  }

  model_items.push(root_item);
  QStack<QDomElement> elements_to_process;

  elements_to_process.push(opml_document.documentElement().elementsByTagName(QSL("body")).at(0).toElement());
  total = opml_document.elementsByTagName(QSL("outline")).size();

  QList<FeedLookup> lookup;

  while (!elements_to_process.isEmpty()) {
    RootItem* active_model_item = model_items.pop();
    QDomElement active_element = elements_to_process.pop();
    int current_count = active_element.childNodes().size();

    for (int i = 0; i < current_count; i++) {
      QDomNode child = active_element.childNodes().at(i);

      if (child.isElement()) {
        QDomElement child_element = child.toElement();

        // Now analyze if this element is category or feed.
        // NOTE: All feeds must include xmlUrl attribute and text attribute.
        if (child_element.attributes().contains(QSL("xmlUrl")) && child.attributes().contains(QSL("text"))) {
          // This is FEED.
          // Add feed and end this iteration.
          QString feed_url = child_element.attribute(QSL("xmlUrl"));

          if (!feed_url.isEmpty()) {
            FeedLookup f;

            f.custom_proxy = custom_proxy;
            f.fetch_metadata_online = fetch_metadata_online;
            f.opml_element = child_element;
            f.parent = active_model_item;
            f.post_process_script = post_process_script;
            f.url = feed_url;

            lookup.append(f);
          }
        }
        else {
          // This must be CATEGORY.
          // Add category and continue.
          QString category_title = child_element.attribute(QSL("text"));
          QString category_description = child_element.attribute(QSL("description"));
          QIcon category_icon =
            qApp->icons()->fromByteArray(child_element.attribute(QSL("rssguard:icon")).toLocal8Bit());

          if (category_title.isEmpty()) {
            qWarningNN << LOGSEC_CORE
                       << "Given OMPL file provided category without valid text attribute. Using fallback name.";
            category_title = child_element.attribute(QSL("title"));

            if (category_title.isEmpty()) {
              category_title = tr("Category ") + QString::number(QDateTime::currentDateTime().toMSecsSinceEpoch());
            }
          }

          auto* new_category = new StandardCategory(active_model_item);

          new_category->setTitle(category_title);

          if (!category_icon.isNull()) {
            new_category->setIcon(category_icon);
          }

          new_category->setDescription(category_description);
          active_model_item->appendChild(new_category);

          // Children of this node must be processed later.
          elements_to_process.push(child_element);
          model_items.push(new_category);
        }

        emit parsingProgress(++completed, total);
      }
    }
  }

  // Now, XML is processed and we have result in form of pointer item structure.
  emit layoutAboutToBeChanged();

  setRootItem(root_item);

  m_lookup.clear();
  m_lookup.append(lookup);

  QFuture<bool> fut = QtConcurrent::mapped(m_lookup, [=](const FeedLookup& lookup) {
    return produceFeed(lookup);
  });

  m_watcherLookup.setFuture(fut);

  if (!fetch_metadata_online) {
    m_watcherLookup.waitForFinished();
  }
}

bool FeedsImportExportModel::exportToTxtURLPerLine(QByteArray& result) {
  auto stf = sourceModel()->rootItem()->getSubTreeFeeds();

  for (const Feed* const feed : qAsConst(stf)) {
    result += feed->source() + QL1S("\n");
  }

  return true;
}

void FeedsImportExportModel::importAsTxtURLPerLine(const QByteArray& data,
                                                   bool fetch_metadata_online,
                                                   const QString& post_process_script) {
  emit parsingStarted();
  emit layoutAboutToBeChanged();
  setRootItem(nullptr);
  emit layoutChanged();

  int completed = 0;
  auto* root_item = new StandardServiceRoot();
  QNetworkProxy custom_proxy;

  if (sourceModel()->rootItem() != nullptr && sourceModel()->rootItem()->getParentServiceRoot() != nullptr) {
    custom_proxy = sourceModel()->rootItem()->getParentServiceRoot()->networkProxy();
  }

  QList<QByteArray> urls = data.split('\n');
  QList<FeedLookup> lookup;

  for (const QByteArray& url : urls) {
    if (!url.isEmpty()) {
      FeedLookup f;

      f.custom_proxy = custom_proxy;
      f.fetch_metadata_online = fetch_metadata_online;
      f.parent = root_item;
      f.post_process_script = post_process_script;
      f.url = url;

      lookup.append(f);
    }
    else {
      qWarningNN << LOGSEC_CORE << "Detected empty URL when parsing input TXT [one URL per line] data.";
    }

    emit parsingProgress(++completed, urls.size());
  }

  // Now, XML is processed and we have result in form of pointer item structure.
  emit layoutAboutToBeChanged();

  setRootItem(root_item);

  m_lookup.clear();
  m_lookup.append(lookup);

  QFuture<bool> fut = QtConcurrent::mapped(m_lookup, [=](const FeedLookup& lookup) {
    return produceFeed(lookup);
  });

  m_watcherLookup.setFuture(fut);

  if (!fetch_metadata_online) {
    m_watcherLookup.waitForFinished();
  }
}

FeedsImportExportModel::Mode FeedsImportExportModel::mode() const {
  return m_mode;
}

void FeedsImportExportModel::setMode(FeedsImportExportModel::Mode mode) {
  m_mode = mode;
}
