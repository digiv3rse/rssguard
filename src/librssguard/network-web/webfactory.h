// For license of this file, see <project-root-folder>/LICENSE.md.

#ifndef WEBFACTORY_H
#define WEBFACTORY_H

#include <QObject>

#include "core/messagesmodel.h"

#include <QMap>

#if defined(USE_WEBENGINE)
#include <QWebEngineSettings>

class NetworkUrlInterceptor;
#endif

class QMenu;
class AdBlockManager;
class CookieJar;
class Readability;

class WebFactory : public QObject {
  Q_OBJECT

  public:
    explicit WebFactory(QObject* parent = nullptr);
    virtual ~WebFactory();

    // Strips "<....>" (HTML, XML) tags from given text.
    QString stripTags(QString text);

    // HTML entity unescaping. This method
    // converts both HTML entity names and numbers to UTF-8 string.
    // Example of entities are:
    //   ∀ = &forall; (entity name), &#8704; (base-10 entity), &#x2200; (base-16 entity)
    QString unescapeHtml(const QString& html);

    QString processFeedUriScheme(const QString& url);

    QAction* engineSettingsAction();
    AdBlockManager* adBlock() const;

#if defined(USE_WEBENGINE)
    NetworkUrlInterceptor* urlIinterceptor() const;
#endif

    CookieJar* cookieJar() const;
    Readability* readability() const;

    void updateProxy();
    bool openUrlInExternalBrowser(const QString& url) const;
    bool sendMessageViaEmail(const Message& message);

#if defined(USE_WEBENGINE)
  private slots:
    void createMenu(QMenu* menu = nullptr);
    void webEngineSettingChanged(bool enabled);

  private:
    QAction* createEngineSettingsAction(const QString& title, QWebEngineSettings::WebAttribute attribute);
#endif

  private:
    void generateUnescapes();

  private:
    AdBlockManager* m_adBlock;

#if defined(USE_WEBENGINE)
    NetworkUrlInterceptor* m_urlInterceptor;
    QAction* m_engineSettings;
#endif

    CookieJar* m_cookieJar;
    Readability* m_readability;
    QMap<QString, char16_t> m_htmlNamedEntities;
};

#endif // WEBFACTORY_H
