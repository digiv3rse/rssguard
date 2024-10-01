// For license of this file, see <project-root-folder>/LICENSE.md.

#ifndef ADBLOCKMANAGER_H
#define ADBLOCKMANAGER_H

#include "miscellaneous/nodejs.h"

#include <QHash>
#include <QObject>
#include <QProcess>

#define CLIQZ_ADBLOCKED_PACKAGE "@cliqz/adblocker"
#define CLIQZ_ADBLOCKED_VERSION "1.27.1"

class QUrl;
class AdblockRequestInfo;
class AdBlockUrlInterceptor;
class AdBlockIcon;

struct BlockingResult {
    bool m_blocked;
    QString m_blockedByFilter;

    BlockingResult() : m_blocked(false), m_blockedByFilter(QString()) {}

    BlockingResult(bool blocked, QString blocked_by_filter = {})
      : m_blocked(blocked), m_blockedByFilter(std::move(blocked_by_filter)) {}
};

class AdBlockManager : public QObject {
    Q_OBJECT

  public:
    explicit AdBlockManager(QObject* parent = nullptr);
    virtual ~AdBlockManager();

    // Enables (or disables) AdBlock feature asynchronously.
    // This method will start/stop AdBlock in separate process
    // and thus cannot run synchronously (when enabling) as process takes
    // some time to start.
    //
    // If the process fails then signal
    //   processTerminated() is thrown.
    // If AdBlock is switched on/off then signal
    //   enabledChanged(bool, QString) is thrown.
    void setEnabled(bool enabled);
    bool isEnabled() const;

    bool canRunOnScheme(const QString& scheme) const;
    AdBlockIcon* adBlockIcon() const;

    // General methods for adblocking.
    BlockingResult block(const AdblockRequestInfo& request);
    QString elementHidingRulesForDomain(const QUrl& url) const;

    QStringList filterLists() const;
    void setFilterLists(const QStringList& filter_lists);

    QStringList customFilters() const;
    void setCustomFilters(const QStringList& custom_filters);

    static QString generateJsForElementHiding(const QString& css);

  public slots:
    void showDialog();

  signals:
    void enabledChanged(bool enabled, QString error = {});
    void processTerminated();

  private slots:
    void onPackageReady(const QObject* sndr, const QList<NodeJs::PackageMetadata>& pkgs, bool already_up_to_date);
    void onPackageError(const QObject* sndr, const QList<NodeJs::PackageMetadata>& pkgs, const QString& error);
    void onServerProcessFinished(int exit_code, QProcess::ExitStatus exit_status);

  private:
    void updateUnifiedFilters();
    void updateUnifiedFiltersFileAndStartServer();

    QProcess* startServer(int port);
    void killServer();

    BlockingResult askServerIfBlocked(const QString& fp_url, const QString& url, const QString& url_type) const;
    QString askServerForCosmeticRules(const QString& url) const;

  private:
    bool m_loaded;
    bool m_enabled;
    bool m_installing;
    AdBlockIcon* m_adblockIcon;

#if defined(NO_LITE)
    AdBlockUrlInterceptor* m_interceptor;
#endif

    QString m_unifiedFiltersFile;
    QProcess* m_serverProcess;
    QHash<QPair<QString, QString>, BlockingResult> m_cacheBlocks;
};

inline AdBlockIcon* AdBlockManager::adBlockIcon() const {
  return m_adblockIcon;
}

#endif // ADBLOCKMANAGER_H
