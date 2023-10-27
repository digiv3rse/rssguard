// For license of this file, see <project-root-folder>/LICENSE.md.

#ifndef FEEDSMODELFEED_H
#define FEEDSMODELFEED_H

#include "services/abstract/feed.h"

#include "network-web/networkfactory.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QMetaType>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QPair>

class StandardServiceRoot;

// Represents BASE class for feeds contained in FeedsModel.
// NOTE: This class should be derived to create PARTICULAR feed types.
class StandardFeed : public Feed {
    Q_OBJECT

    friend class StandardCategory;

  public:
    enum class SourceType {
      Url = 0,
      Script = 1,
      LocalFile = 2
    };

    enum class Type {
      Rss0X = 0,
      Rss2X = 1,
      Rdf = 2, // Sometimes denoted as RSS 1.0.
      Atom10 = 3,
      Json = 4,
      Sitemap = 5
    };

    explicit StandardFeed(RootItem* parent_item = nullptr);
    explicit StandardFeed(const StandardFeed& other);

    virtual QList<QAction*> contextMenuFeedsList();
    virtual QString additionalTooltip() const;
    virtual bool canBeDeleted() const;
    virtual bool deleteViaGui();
    virtual QVariantHash customDatabaseData() const;
    virtual void setCustomDatabaseData(const QVariantHash& data);
    virtual Qt::ItemFlags additionalFlags() const;
    virtual bool performDragDropChange(RootItem* target_item);

    // Other getters/setters.
    Type type() const;
    void setType(Type type);

    SourceType sourceType() const;
    void setSourceType(SourceType source_type);

    QString encoding() const;
    void setEncoding(const QString& encoding);

    QString postProcessScript() const;
    void setPostProcessScript(const QString& post_process_script);

    NetworkFactory::NetworkAuthentication protection() const;
    void setProtection(NetworkFactory::NetworkAuthentication protect);

    QString username() const;
    void setUsername(const QString& username);

    QString password() const;
    void setPassword(const QString& password);

    // Tries to guess feed hidden under given URL
    // and uses given credentials.
    // Returns pointer to guessed feed (if at least partially
    // guessed) and retrieved error/status code from network layer
    // or nullptr feed.
    static StandardFeed* guessFeed(SourceType source_type,
                                   const QString& url,
                                   const QString& post_process_script,
                                   NetworkFactory::NetworkAuthentication protection,
                                   bool fetch_icons = true,
                                   const QString& username = {},
                                   const QString& password = {},
                                   const QNetworkProxy& custom_proxy = QNetworkProxy::ProxyType::DefaultProxy);

    // Converts particular feed type to string.
    static QString typeToString(Type type);
    static QString sourceTypeToString(SourceType type);

    // Scraping + post+processing.
    static QStringList prepareExecutionLine(const QString& execution_line);
    static QByteArray generateFeedFileWithScript(const QString& execution_line, int run_timeout);
    static QByteArray postProcessFeedFileWithScript(const QString& execution_line,
                                                    const QString& raw_feed_data,
                                                    int run_timeout);
    static QByteArray runScriptProcess(const QStringList& cmd_args,
                                       const QString& working_directory,
                                       int run_timeout,
                                       bool provide_input,
                                       const QString& input = {});

    QString lastEtag() const;
    void setLastEtag(const QString& etag);

  public slots:
    void fetchMetadataForItself();

  private:
    StandardServiceRoot* serviceRoot() const;
    bool removeItself();

  private:
    SourceType m_sourceType;
    Type m_type;
    QString m_postProcessScript;
    QString m_encoding;
    NetworkFactory::NetworkAuthentication m_protection = NetworkFactory::NetworkAuthentication::NoAuthentication;
    QString m_username;
    QString m_password;
    QString m_lastEtag;
};

Q_DECLARE_METATYPE(StandardFeed::SourceType)
Q_DECLARE_METATYPE(StandardFeed::Type)

#endif // FEEDSMODELFEED_H
