// For license of this file, see <project-root-folder>/LICENSE.md.

#include "core/feeddownloader.h"

#include "3rd-party/boolinq/boolinq.h"
#include "core/feedsmodel.h"
#include "core/messagefilter.h"
#include "database/databasequeries.h"
#include "definitions/definitions.h"
#include "exceptions/feedfetchexception.h"
#include "exceptions/filteringexception.h"
#include "miscellaneous/application.h"
#include "services/abstract/cacheforserviceroot.h"
#include "services/abstract/feed.h"
#include "services/abstract/labelsnode.h"

#include <QDebug>
#include <QJSEngine>
#include <QMutexLocker>
#include <QString>
#include <QThread>

FeedDownloader::FeedDownloader()
  : QObject(), m_isCacheSynchronizationRunning(false), m_stopCacheSynchronization(false), m_mutex(new QMutex()), m_feedsUpdated(0), m_feedsOriginalCount(0) {
  qRegisterMetaType<FeedDownloadResults>("FeedDownloadResults");
}

FeedDownloader::~FeedDownloader() {
  m_mutex->tryLock();
  m_mutex->unlock();
  delete m_mutex;
  qDebugNN << LOGSEC_FEEDDOWNLOADER << "Destroying FeedDownloader instance.";
}

bool FeedDownloader::isUpdateRunning() const {
  return !m_feeds.isEmpty();
}

void FeedDownloader::synchronizeAccountCaches(const QList<CacheForServiceRoot*>& caches, bool emit_signals) {
  m_isCacheSynchronizationRunning = true;

  for (CacheForServiceRoot* cache : caches) {
    qDebugNN << LOGSEC_FEEDDOWNLOADER
             << "Synchronizing cache back to server on thread" << QUOTE_W_SPACE_DOT(QThread::currentThreadId());
    cache->saveAllCachedData(false);

    if (m_stopCacheSynchronization) {
      qWarningNN << LOGSEC_FEEDDOWNLOADER << "Aborting cache synchronization.";

      m_stopCacheSynchronization = false;
      break;
    }
  }

  m_isCacheSynchronizationRunning = false;
  qDebugNN << LOGSEC_FEEDDOWNLOADER << "All caches synchronized.";

  if (emit_signals) {
    emit cachesSynchronized();
  }
}

void FeedDownloader::updateFeeds(const QList<Feed*>& feeds) {
  QMutexLocker locker(m_mutex);

  m_results.clear();
  m_feeds = feeds;
  m_feedsOriginalCount = m_feeds.size();
  m_feedsUpdated = 0;

  if (feeds.isEmpty()) {
    qDebugNN << LOGSEC_FEEDDOWNLOADER << "No feeds to update in worker thread, aborting update.";
  }
  else {
    qDebugNN << LOGSEC_FEEDDOWNLOADER
             << "Starting feed updates from worker in thread: '"
             << QThread::currentThreadId() << "'.";

    // Job starts now.
    emit updateStarted();
    QSet<CacheForServiceRoot*> caches;
    QMultiHash<ServiceRoot*, Feed*> feeds_per_root;

    // 1. key - account.
    // 2. key - feed custom ID.
    // 3. key - msg state.
    QHash<ServiceRoot*, QHash<QString, QHash<ServiceRoot::BagOfMessages, QStringList>>> stated_messages;

    // 1. key - account.
    // 2. key - label custom ID.
    QHash<ServiceRoot*, QHash<QString, QStringList>> tagged_messages;

    for (auto* fd : feeds) {
      CacheForServiceRoot* fd_cache = fd->getParentServiceRoot()->toCache();

      if (fd_cache != nullptr) {
        caches.insert(fd_cache);
      }

      feeds_per_root.insert(fd->getParentServiceRoot(), fd);
    }

    synchronizeAccountCaches(caches.values(), false);

    auto roots = feeds_per_root.uniqueKeys();
    bool is_main_thread = QThread::currentThread() == qApp->thread();
    QSqlDatabase database = is_main_thread ?
                            qApp->database()->driver()->connection(metaObject()->className()) :
                            qApp->database()->driver()->connection(QSL("feed_upd"));

    for (auto* rt : roots) {
      // Obtain lists of local IDs.
      if (rt->wantsBaggedIdsOfExistingMessages()) {
        // Tagged messages for the account.
        tagged_messages.insert(rt, DatabaseQueries::bagsOfMessages(database, rt->labelsNode()->labels()));

        QHash<QString, QHash<ServiceRoot::BagOfMessages, QStringList>> per_acc_states;

        // This account has activated intelligent downloading of messages.
        // Prepare bags.
        auto fds = feeds_per_root.values(rt);

        for (Feed* fd : fds) {
          QHash<ServiceRoot::BagOfMessages, QStringList> per_feed_states;

          per_feed_states.insert(ServiceRoot::BagOfMessages::Read,
                                 DatabaseQueries::bagOfMessages(database,
                                                                ServiceRoot::BagOfMessages::Read,
                                                                fd));
          per_feed_states.insert(ServiceRoot::BagOfMessages::Unread,
                                 DatabaseQueries::bagOfMessages(database,
                                                                ServiceRoot::BagOfMessages::Unread,
                                                                fd));
          per_feed_states.insert(ServiceRoot::BagOfMessages::Starred,
                                 DatabaseQueries::bagOfMessages(database,
                                                                ServiceRoot::BagOfMessages::Starred,
                                                                fd));
          per_acc_states.insert(fd->customId(), per_feed_states);
        }

        stated_messages.insert(rt, per_acc_states);
      }

      rt->aboutToBeginFeedFetching(feeds_per_root.values(rt),
                                   stated_messages.value(rt),
                                   tagged_messages.value(rt));
    }

    while (!m_feeds.isEmpty()) {
      auto n_f = m_feeds.takeFirst();

      updateOneFeed(n_f->getParentServiceRoot(),
                    n_f,
                    stated_messages.value(n_f->getParentServiceRoot()).value(n_f->customId()),
                    tagged_messages.value(n_f->getParentServiceRoot()));
    }
  }

  finalizeUpdate();
}

void FeedDownloader::stopRunningUpdate() {
  m_stopCacheSynchronization = true;
  m_feeds.clear();
  m_feedsOriginalCount = m_feedsUpdated = 0;
}

void FeedDownloader::updateOneFeed(ServiceRoot* acc,
                                   Feed* feed,
                                   const QHash<ServiceRoot::BagOfMessages, QStringList>& stated_messages,
                                   const QHash<QString, QStringList>& tagged_messages) {
  qDebugNN << LOGSEC_FEEDDOWNLOADER
           << "Downloading new messages for feed ID '"
           << feed->customId() << "' URL: '" << feed->source() << "' title: '" << feed->title() << "' in thread: '"
           << QThread::currentThreadId() << "'.";

  int acc_id = feed->getParentServiceRoot()->accountId();
  QElapsedTimer tmr; tmr.start();

  try {
    bool is_main_thread = QThread::currentThread() == qApp->thread();
    QSqlDatabase database = is_main_thread ?
                            qApp->database()->driver()->connection(metaObject()->className()) :
                            qApp->database()->driver()->connection(QSL("feed_upd"));
    QList<Message> msgs = feed->getParentServiceRoot()->obtainNewMessages(feed,
                                                                          stated_messages,
                                                                          tagged_messages);

    qDebugNN << LOGSEC_FEEDDOWNLOADER << "Downloaded " << msgs.size() << " messages for feed ID '"
             << feed->customId() << "' URL: '" << feed->source() << "' title: '" << feed->title() << "' in thread: '"
             << QThread::currentThreadId() << "'. Operation took " << tmr.nsecsElapsed() / 1000 << " microseconds.";

    bool fix_future_datetimes = qApp->settings()->value(GROUP(Messages),
                                                        SETTING(Messages::FixupFutureArticleDateTimes)).toBool();

    // Now, sanitize messages (tweak encoding etc.).
    for (auto& msg : msgs) {
      msg.m_accountId = acc_id;
      msg.sanitize(feed, fix_future_datetimes);
    }

    if (!feed->messageFilters().isEmpty()) {
      tmr.restart();

      // Perform per-message filtering.
      QJSEngine filter_engine;

      // Create JavaScript communication wrapper for the message.
      MessageObject msg_obj(&database,
                            feed->customId(),
                            feed->getParentServiceRoot()->accountId(),
                            feed->getParentServiceRoot()->labelsNode()->labels(),
                            true);

      MessageFilter::initializeFilteringEngine(filter_engine, &msg_obj);

      qDebugNN << LOGSEC_FEEDDOWNLOADER << "Setting up JS evaluation took " << tmr.nsecsElapsed() / 1000 << " microseconds.";

      QList<Message> read_msgs, important_msgs;

      for (int i = 0; i < msgs.size(); i++) {
        Message msg_backup(msgs[i]);
        Message* msg_orig = &msgs[i];

        // Attach live message object to wrapper.
        tmr.restart();
        msg_obj.setMessage(msg_orig);
        qDebugNN << LOGSEC_FEEDDOWNLOADER << "Hooking message took " << tmr.nsecsElapsed() / 1000 << " microseconds.";

        auto feed_filters = feed->messageFilters();
        bool remove_msg = false;

        for (int j = 0; j < feed_filters.size(); j++) {
          QPointer<MessageFilter> filter = feed_filters.at(j);

          if (filter.isNull()) {
            qCriticalNN << LOGSEC_FEEDDOWNLOADER
                        << "Article filter was probably deleted, removing its pointer from list of filters.";
            feed_filters.removeAt(j--);
            continue;
          }

          MessageFilter* msg_filter = filter.data();

          tmr.restart();

          try {
            MessageObject::FilteringAction decision = msg_filter->filterMessage(&filter_engine);

            qDebugNN << LOGSEC_FEEDDOWNLOADER
                     << "Running filter script, it took " << tmr.nsecsElapsed() / 1000 << " microseconds.";

            switch (decision) {
              case MessageObject::FilteringAction::Accept:
                // Message is normally accepted, it could be tweaked by the filter.
                continue;

              case MessageObject::FilteringAction::Ignore:
              case MessageObject::FilteringAction::Purge:
              default:
                // Remove the message, we do not want it.
                remove_msg = true;
                break;
            }
          }
          catch (const FilteringException& ex) {
            qCriticalNN << LOGSEC_FEEDDOWNLOADER
                        << "Error when evaluating filtering JS function: "
                        << QUOTE_W_SPACE_DOT(ex.message())
                        << " Accepting message.";
            continue;
          }

          // If we reach this point. Then we ignore the message which is by now
          // already removed, go to next message.
          break;
        }

        if (!msg_backup.m_isRead && msg_orig->m_isRead) {
          qDebugNN << LOGSEC_FEEDDOWNLOADER << "Message with custom ID: '" << msg_backup.m_customId << "' was marked as read by message scripts.";

          read_msgs << *msg_orig;
        }

        if (!msg_backup.m_isImportant && msg_orig->m_isImportant) {
          qDebugNN << LOGSEC_FEEDDOWNLOADER << "Message with custom ID: '" << msg_backup.m_customId << "' was marked as important by message scripts.";

          important_msgs << *msg_orig;
        }

        // Process changed labels.
        for (Label* lbl : qAsConst(msg_backup.m_assignedLabels)) {
          if (!msg_orig->m_assignedLabels.contains(lbl)) {
            // Label is not there anymore, it was deassigned.
            lbl->deassignFromMessage(*msg_orig);

            qDebugNN << LOGSEC_FEEDDOWNLOADER
                     << "It was detected that label" << QUOTE_W_SPACE(lbl->customId())
                     << "was DEASSIGNED from message" << QUOTE_W_SPACE(msg_orig->m_customId)
                     << "by message filter(s).";
          }
        }

        for (Label* lbl : qAsConst(msg_orig->m_assignedLabels)) {
          if (!msg_backup.m_assignedLabels.contains(lbl)) {
            // Label is in new message, but is not in old message, it
            // was newly assigned.
            lbl->assignToMessage(*msg_orig);

            qDebugNN << LOGSEC_FEEDDOWNLOADER
                     << "It was detected that label" << QUOTE_W_SPACE(lbl->customId())
                     << "was ASSIGNED to message" << QUOTE_W_SPACE(msg_orig->m_customId)
                     << "by message filter(s).";
          }
        }

        if (remove_msg) {
          msgs.removeAt(i--);
        }
      }

      if (!read_msgs.isEmpty()) {
        // Now we push new read states to the service.
        if (feed->getParentServiceRoot()->onBeforeSetMessagesRead(feed, read_msgs, RootItem::ReadStatus::Read)) {
          qDebugNN << LOGSEC_FEEDDOWNLOADER
                   << "Notified services about messages marked as read by message filters.";
        }
        else {
          qCriticalNN << LOGSEC_FEEDDOWNLOADER
                      << "Notification of services about messages marked as read by message filters FAILED.";
        }
      }

      if (!important_msgs.isEmpty()) {
        // Now we push new read states to the service.
        auto list = boolinq::from(important_msgs).select([](const Message& msg) {
          return ImportanceChange(msg, RootItem::Importance::Important);
        }).toStdList();
        QList<ImportanceChange> chngs = FROM_STD_LIST(QList<ImportanceChange>, list);

        if (feed->getParentServiceRoot()->onBeforeSwitchMessageImportance(feed, chngs)) {
          qDebugNN << LOGSEC_FEEDDOWNLOADER
                   << "Notified services about messages marked as important by message filters.";
        }
        else {
          qCriticalNN << LOGSEC_FEEDDOWNLOADER
                      << "Notification of services about messages marked as important by message filters FAILED.";
        }
      }
    }

    removeDuplicateMessages(msgs);

    // Now make sure, that messages are actually stored to SQL in a locked state.
    qDebugNN << LOGSEC_FEEDDOWNLOADER << "Saving messages of feed ID '"
             << feed->customId() << "' URL: '" << feed->source() << "' title: '" << feed->title() << "' in thread: '"
             << QThread::currentThreadId() << "'.";

    tmr.restart();
    auto updated_messages = acc->updateMessages(msgs, feed, false);

    qDebugNN << LOGSEC_FEEDDOWNLOADER
             << "Updating messages in DB took " << tmr.nsecsElapsed() / 1000 << " microseconds.";

    if (feed->status() != Feed::Status::NewMessages) {
      feed->setStatus(updated_messages.first > 0 || updated_messages.second > 0
                ? Feed::Status::NewMessages
                : Feed::Status::Normal);
    }

    qDebugNN << LOGSEC_FEEDDOWNLOADER
             << updated_messages << " messages for feed "
             << feed->customId() << " stored in DB.";

    if (updated_messages.first > 0) {
      m_results.appendUpdatedFeed({ feed->title(), updated_messages.first });
    }
  }
  catch (const FeedFetchException& feed_ex) {
    qCriticalNN << LOGSEC_NETWORK
                << "Error when fetching feed:"
                << QUOTE_W_SPACE(feed_ex.feedStatus())
                << "message:"
                << QUOTE_W_SPACE_DOT(feed_ex.message());

    feed->setStatus(feed_ex.feedStatus(), feed_ex.message());
  }
  catch (const ApplicationException& app_ex) {
    qCriticalNN << LOGSEC_NETWORK
                << "Unknown error when fetching feed:"
                << "message:"
                << QUOTE_W_SPACE_DOT(app_ex.message());

    feed->setStatus(Feed::Status::OtherError, app_ex.message());
  }

  feed->getParentServiceRoot()->itemChanged({ feed });

  m_feedsUpdated++;

  qDebugNN << LOGSEC_FEEDDOWNLOADER
           << "Made progress in feed updates, total feeds count "
           << m_feedsUpdated << "/" << m_feedsOriginalCount << " (id of feed is "
           << feed->id() << ").";
  emit updateProgress(feed, m_feedsUpdated, m_feedsOriginalCount);
}

void FeedDownloader::finalizeUpdate() {
  qDebugNN << LOGSEC_FEEDDOWNLOADER << "Finished feed updates in thread: '" << QThread::currentThreadId() << "'.";
  m_results.sort();

  // Update of feeds has finished.
  // NOTE: This means that now "update lock" can be unlocked
  // and feeds can be added/edited/deleted and application
  // can eventually quit.
  emit updateFinished(m_results);
}

bool FeedDownloader::isCacheSynchronizationRunning() const {
  return m_isCacheSynchronizationRunning;
}

void FeedDownloader::removeDuplicateMessages(QList<Message>& messages) {
  auto idx = 0;

  while (idx < messages.size()) {
    Message& message = messages[idx];
    std::function<bool(const Message& a, const Message& b)> is_duplicate;

    if (message.m_id > 0) {
      is_duplicate = [](const Message& a, const Message& b) {
                       return a.m_id == b.m_id;
                     };
    }
    else if (message.m_customId.isEmpty()) {
      is_duplicate = [](const Message& a, const Message& b) {
                       return std::tie(a.m_title, a.m_url, a.m_author) == std::tie(b.m_title, b.m_url, b.m_author);
                     };
    }
    else {
      is_duplicate = [](const Message& a, const Message& b) {
                       return a.m_customId == b.m_customId;
                     };
    }

    auto next_idx = idx + 1; // Index of next message to check after removing all duplicates.
    auto last_idx = idx; // Index of the last kept duplicate.

    idx = next_idx;

    // Remove all duplicate messages, and keep the message with the latest created date.
    // If the created date is identical for all duplicate messages then keep the last message in the list.
    while (idx < messages.size()) {
      auto& last_duplicate = messages[last_idx];

      if (is_duplicate(last_duplicate, messages[idx])) {
        if (last_duplicate.m_created <= messages[idx].m_created) {
          // The last seen message was created earlier or at the same date -- keep the current, and remove the last.
          qWarningNN << LOGSEC_CORE << "Removing article" << QUOTE_W_SPACE(last_duplicate.m_title)
                     << "before saving articles to DB, because it is duplicate.";

          messages.removeAt(last_idx);
          if (last_idx + 1 == next_idx) {
            // The `next_idx` was pointing to the message following the duplicate. With that duplicate removed the
            // next index needs to be adjusted.
            next_idx = last_idx;
          }

          last_idx = idx;
          ++idx;
        }
        else {
          qWarningNN << LOGSEC_CORE << "Removing article" << QUOTE_W_SPACE(messages[idx].m_title)
                     << "before saving articles to DB, because it is duplicate.";

          messages.removeAt(idx);
        }
      }
      else {
        ++idx;
      }
    }

    idx = next_idx;
  }
}

QString FeedDownloadResults::overview(int how_many_feeds) const {
  QStringList result;

  for (int i = 0, number_items_output = qMin(how_many_feeds, m_updatedFeeds.size()); i < number_items_output; i++) {
    result.append(m_updatedFeeds.at(i).first + QSL(": ") + QString::number(m_updatedFeeds.at(i).second));
  }

  QString res_str = result.join(QSL("\n"));

  if (m_updatedFeeds.size() > how_many_feeds) {
    res_str += QObject::tr("\n\n+ %n other feeds.", nullptr, m_updatedFeeds.size() - how_many_feeds);
  }

  return res_str;
}

void FeedDownloadResults::appendUpdatedFeed(const QPair<QString, int>& feed) {
  m_updatedFeeds.append(feed);
}

void FeedDownloadResults::sort() {
  std::sort(m_updatedFeeds.begin(), m_updatedFeeds.end(), [](const QPair<QString, int>& lhs, const QPair<QString, int>& rhs) {
    return lhs.second > rhs.second;
  });
}

void FeedDownloadResults::clear() {
  m_updatedFeeds.clear();
}

QList<QPair<QString, int>> FeedDownloadResults::updatedFeeds() const {
  return m_updatedFeeds;
}
