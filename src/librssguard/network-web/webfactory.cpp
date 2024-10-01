// For license of this file, see <project-root-folder>/LICENSE.md.

#include "network-web/webfactory.h"

#include "gui/messagebox.h"
#include "miscellaneous/application.h"
#include "miscellaneous/iconfactory.h"
#include "miscellaneous/settings.h"
#include "network-web/adblock/adblockmanager.h"
#include "network-web/apiserver.h"
#include "network-web/articleparse.h"
#include "network-web/cookiejar.h"
#include "network-web/readability.h"

#include <QDesktopServices>
#include <QProcess>
#include <QUrl>

#if defined(NO_LITE)
#include "network-web/webengine/networkurlinterceptor.h"

#if QT_VERSION_MAJOR == 6
#include <QWebEngineDownloadRequest>
#else
#include <QWebEngineDownloadItem>
#endif

#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineUrlScheme>
#endif

WebFactory::WebFactory(QObject* parent) : QObject(parent), m_apiServer(nullptr), m_customUserAgent(QString()) {
  m_adBlock = new AdBlockManager(this);

  if (qApp->settings()->value(GROUP(Network), SETTING(Network::EnableApiServer)).toBool()) {
    startApiServer();
  }

#if defined(NO_LITE)
  if (qApp->settings()->value(GROUP(Browser), SETTING(Browser::DisableCache)).toBool()) {
    qWarningNN << LOGSEC_NETWORK << "Using off-the-record WebEngine profile.";

    m_engineProfile = new QWebEngineProfile(this);
  }
  else {
    m_engineProfile = new QWebEngineProfile(QSL(APP_LOW_NAME), this);
  }

  m_engineSettings = nullptr;
  m_urlInterceptor = new NetworkUrlInterceptor(this);
#endif

  m_cookieJar = new CookieJar(this);
  m_readability = new Readability(this);
  m_articleParse = new ArticleParse(this);

#if defined(NO_LITE)
#if QT_VERSION >= 0x050D00 // Qt >= 5.13.0
  m_engineProfile->setUrlRequestInterceptor(m_urlInterceptor);
#else
  m_engineProfile->setRequestInterceptor(m_urlInterceptor);
#endif
#endif
}

WebFactory::~WebFactory() {
  stopApiServer();

#if defined(NO_LITE)
  if (m_engineSettings != nullptr && m_engineSettings->menu() != nullptr) {
    m_engineSettings->menu()->deleteLater();
  }
#endif

  /*if (m_cookieJar != nullptr) {
    m_cookieJar->deleteLater();
  }*/
}

bool WebFactory::sendMessageViaEmail(const Message& message) {
  if (qApp->settings()->value(GROUP(Browser), SETTING(Browser::CustomExternalEmailEnabled)).toBool()) {
    const QString browser =
      qApp->settings()->value(GROUP(Browser), SETTING(Browser::CustomExternalEmailExecutable)).toString();
    const QString arguments =
      qApp->settings()->value(GROUP(Browser), SETTING(Browser::CustomExternalEmailArguments)).toString();
    const QStringList tokenized_arguments =
      TextFactory::tokenizeProcessArguments(arguments.arg(message.m_title, stripTags(message.m_contents)));

    return IOFactory::startProcessDetached(browser, tokenized_arguments);
  }
  else {
    // Send it via mailto protocol.
    // NOTE: http://en.wikipedia.org/wiki/Mailto
    return QDesktopServices::openUrl(QSL("mailto:?subject=%1&body=%2")
                                       .arg(QString(QUrl::toPercentEncoding(message.m_title)),
                                            QString(QUrl::toPercentEncoding(stripTags(message.m_contents)))));
  }
}

#if defined(NO_LITE)
void WebFactory::loadCustomCss(const QString user_styles_path) {
  if (QFile::exists(user_styles_path)) {
    QByteArray css_data = IOFactory::readFile(user_styles_path);
    QString name = "rssguard-user-styles";
    QWebEngineScript script;
    QString s = QSL("(function() {"
                    "  css = document.createElement('style');"
                    "  css.type = 'text/css';"
                    "  css.id = '%1';"
                    "  document.head.appendChild(css);"
                    "  css.innerText = '%2';"
                    "})()")
                  .arg(name, css_data.simplified());
    script.setName(name);
    script.setSourceCode(s);
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    script.setRunsOnSubFrames(false);
    script.setWorldId(QWebEngineScript::ApplicationWorld);

    m_engineProfile->scripts()->insert(script);

    qDebugNN << LOGSEC_CORE << "Loading user CSS style file" << QUOTE_W_SPACE_DOT(user_styles_path);
  }
  else {
    qWarningNN << LOGSEC_CORE << "User CSS style was not provided in file" << QUOTE_W_SPACE_DOT(user_styles_path);
  }
}
#endif

bool WebFactory::openUrlInExternalBrowser(const QUrl& url) const {
  QString my_url = url.toString(QUrl::ComponentFormattingOption::FullyEncoded);

  qDebugNN << LOGSEC_NETWORK << "We are trying to open URL" << QUOTE_W_SPACE_DOT(my_url);

  bool result = false;

  if (qApp->settings()->value(GROUP(Browser), SETTING(Browser::CustomExternalBrowserEnabled)).toBool()) {
    const QString browser =
      qApp->settings()->value(GROUP(Browser), SETTING(Browser::CustomExternalBrowserExecutable)).toString();
    const QString arguments =
      qApp->settings()->value(GROUP(Browser), SETTING(Browser::CustomExternalBrowserArguments)).toString();
    const auto nice_args = arguments.arg(my_url);

    qDebugNN << LOGSEC_NETWORK << "Arguments for external browser:" << QUOTE_W_SPACE_DOT(nice_args);

    result = IOFactory::startProcessDetached(browser, TextFactory::tokenizeProcessArguments(nice_args));

    if (!result) {
      qDebugNN << LOGSEC_NETWORK << "External web browser call failed.";
    }
  }
  else {
    result = QDesktopServices::openUrl(my_url);
  }

  if (!result) {
    // We display GUI information that browser was not probably opened.
    MsgBox::show(qApp->mainFormWidget(),
                 QMessageBox::Icon::Critical,
                 tr("Navigate to website manually"),
                 tr("%1 was unable to launch your web browser with the given URL, you need to open the "
                    "below website URL in your web browser manually.")
                   .arg(QSL(APP_NAME)),
                 {},
                 my_url,
                 QMessageBox::StandardButton::Ok);
  }

  return result;
}

QString WebFactory::stripTags(QString text) {
  static QRegularExpression reg_tags(QSL("<[^>]*>"));

  return text.remove(reg_tags);
}

QString WebFactory::unescapeHtml(const QString& html) {
  if (html.isEmpty()) {
    return html;
  }

  static QMap<QString, char16_t> entities = generateUnescapes();

  QString output;
  output.reserve(html.size());

  // Traverse input HTML string and replace named/number entities.
  for (int pos = 0; pos < html.size();) {
    const QChar first = html.at(pos);

    if (first == QChar('&')) {
      // We need to find ending ';'.
      int pos_end = -1;

      // We're finding end of entity, but also we limit searching window to 10 characters.
      for (int pos_find = pos; pos_find <= pos + 10 && pos_find < html.size(); pos_find++) {
        if (html.at(pos_find) == QChar(';')) {
          // We found end of the entity.
          pos_end = pos_find;
          break;
        }
      }

      if (pos_end + 1 > pos) {
        // OK, we have entity.
        if (html.at(pos + 1) == QChar('#')) {
          // We have numbered entity.
          uint number;
          QString number_str;

          if (html.at(pos + 2) == QChar('x')) {
            // base-16 number.
            number_str = html.mid(pos + 3, pos_end - pos - 3);
            number = number_str.toUInt(nullptr, 16);
          }
          else {
            // base-10 number.
            number_str = html.mid(pos + 2, pos_end - pos - 2);
            number = number_str.toUInt();
          }

          if (number > 0U) {
            output.append(QString::fromUcs4((const char32_t*)&number, 1));
          }
          else {
            // Failed to convert to number, leave intact.
            output.append(html.mid(pos, pos_end - pos + 1));
          }

          pos = pos_end + 1;
          continue;
        }
        else {
          // We have named entity.
          auto entity_name = html.mid(pos + 1, pos_end - pos - 1);

          if (entities.contains(entity_name)) {
            // Entity found, proceed.
            output.append(entities.value(entity_name));
          }
          else {
            // Entity NOT found, leave intact.
            output.append('&');
            output.append(entity_name);
            output.append(';');
          }

          pos = pos_end + 1;
          continue;
        }
      }
    }

    // No entity, normally append and continue.
    output.append(first);
    pos++;
  }

  /*
     qDebugNN << LOGSEC_CORE
           << "Unescaped string" << QUOTE_W_SPACE(html)
           << "to" << QUOTE_W_SPACE_DOT(output);
   */

  return output;
}

QString WebFactory::limitSizeOfHtmlImages(const QString& html, int desired_width, int desired_max_height) const {
  static QRegularExpression exp_image_tag(QSL("<img ([^>]+)>"));
  static QRegularExpression exp_image_attrs(QSL("(\\w+)=\"([^\"]+)\""));
  static bool is_lite = qApp->usingLite();

  // Replace too big pictures. What it exactly does:
  //  - find all <img> tags and check for existence of height/width attributes:
  //    - both found -> keep aspect ratio and change to fit width if too big (or limit height if configured)
  //    - height found only -> limit height if configured
  //    - width found only -> change to fit width if too big
  //    - nothing found (image dimensions are taken directly from picture) -> limit height if configured,
  QRegularExpressionMatch exp_match;
  qsizetype match_offset = 0;
  QString my_html = html;
  QElapsedTimer tmr;

#if !defined(NDEBUG)
  // IOFactory::writeFile("a.html", html.toUtf8());
#endif

  tmr.start();

  while ((exp_match = exp_image_tag.match(my_html, match_offset)).hasMatch()) {
    QString img_reconstructed = QSL("<img");

    // QString full = exp_match.captured();
    // auto aa = exp_match.capturedLength();

    QString img_tag_inner_text = exp_match.captured(1);

    // We found image, now we parse its attributes and process them.
    QRegularExpressionMatchIterator attrs_match_iter = exp_image_attrs.globalMatch(img_tag_inner_text);
    QMap<QString, QString> attrs;

    while (attrs_match_iter.hasNext()) {
      QRegularExpressionMatch attrs_match = attrs_match_iter.next();

      QString attr_name = attrs_match.captured(1);
      QString attr_value = attrs_match.captured(2);

      attrs.insert(attr_name, attr_value);
    }

    // Now, we edit height/width differently, depending whether this is
    // simpler HTML (lite) viewer, or WebEngine full-blown viewer.
    if (is_lite) {
      if (attrs.contains("height") && attrs.contains("width")) {
        double ratio = attrs.value("width").toDouble() / attrs.value("height").toDouble();

        if (desired_max_height > 0) {
          // We limit height.
          attrs.insert("height", QString::number(desired_max_height));
          attrs.insert("width", QString::number(int(ratio * desired_max_height)));
        }

        // We fit width.
        if (attrs.value("width").toInt() > desired_width) {
          attrs.insert("width", QString::number(desired_width));
          attrs.insert("height", QString::number(int(desired_width / ratio)));
        }
      }
      else if (attrs.contains("width")) {
        // Only width.
        if (attrs.value("width").toInt() > desired_width) {
          attrs.insert("width", QString::number(desired_width));
        }
      }
      else {
        // No dimensions given.
        // In this case we simply rely on original image dimensions
        // if no specific limit is set.
        // Too wide images will get downscaled.
        if (desired_max_height > 0) {
          attrs.insert("height", QString::number(desired_max_height));
        }
      }
    }
    else {
      attrs.remove("width");
      attrs.remove("height");

      if (desired_max_height > 0) {
        attrs.insert("style", QSL("max-height: %1px !important;").arg(desired_max_height));
      }
    }

    // Re-insert all attributes.
    while (!attrs.isEmpty()) {
      auto first_key = attrs.firstKey();
      auto first_value = attrs.first();

      img_reconstructed += QSL(" %1=\"%2\"").arg(first_key, first_value);

      attrs.remove(first_key);
    }

    img_reconstructed += QSL(">");

    my_html = my_html.replace(exp_match.capturedStart(), exp_match.capturedLength(), img_reconstructed);

    /*if (found_width > desired_width) {
      qWarningNN << LOGSEC_GUI << "Element" << QUOTE_W_SPACE(exp_match.captured())
                 << "is too wide, setting smaller value to prevent horizontal scrollbars.";

      my_html =
        my_html.replace(exp_match.capturedStart(1), exp_match.capturedLength(1), QString::number(desired_width));
    }*/

    match_offset = exp_match.capturedStart() + img_reconstructed.size();
  }

#if !defined(NDEBUG)
  // IOFactory::writeFile("b.html", my_html.toUtf8());
#endif

  qDebugNN << LOGSEC_GUI << "HTML image resizing took" << NONQUOTE_W_SPACE(tmr.elapsed()) << "miliseconds.";
  return my_html;
}

QString WebFactory::processFeedUriScheme(const QString& url) {
  if (url.startsWith(QSL(URI_SCHEME_FEED))) {
    return QSL(URI_SCHEME_HTTPS) + url.mid(QSL(URI_SCHEME_FEED).size());
  }
  else if (url.startsWith(QSL(URI_SCHEME_FEED_SHORT))) {
    return url.mid(QSL(URI_SCHEME_FEED_SHORT).size());
  }
  else {
    return url;
  }
}

void WebFactory::updateProxy() {
  const QNetworkProxy::ProxyType selected_proxy_type =
    static_cast<QNetworkProxy::ProxyType>(qApp->settings()->value(GROUP(Proxy), SETTING(Proxy::Type)).toInt());

  if (selected_proxy_type == QNetworkProxy::NoProxy) {
    qDebugNN << LOGSEC_NETWORK << "Disabling application-wide proxy completely.";

    QNetworkProxyFactory::setUseSystemConfiguration(false);
    QNetworkProxy::setApplicationProxy(QNetworkProxy::ProxyType::NoProxy);
  }
  else if (selected_proxy_type == QNetworkProxy::ProxyType::DefaultProxy) {
    qDebugNN << LOGSEC_NETWORK << "Using application-wide proxy to be system's default proxy.";
    QNetworkProxyFactory::setUseSystemConfiguration(true);
  }
  else {
    const Settings* settings = qApp->settings();
    QNetworkProxy new_proxy;

    // Custom proxy is selected, set it up.
    new_proxy.setType(selected_proxy_type);
    new_proxy.setHostName(settings->value(GROUP(Proxy), SETTING(Proxy::Host)).toString());
    new_proxy.setPort(quint16(settings->value(GROUP(Proxy), SETTING(Proxy::Port)).toInt()));
    new_proxy.setUser(settings->value(GROUP(Proxy), SETTING(Proxy::Username)).toString());
    new_proxy.setPassword(settings->password(GROUP(Proxy), SETTING(Proxy::Password)).toString());

    qWarningNN << LOGSEC_NETWORK
               << "Activating application-wide custom proxy, address:" << QUOTE_W_SPACE_COMMA(new_proxy.hostName())
               << " type:" << QUOTE_W_SPACE_DOT(new_proxy.type());

    QNetworkProxy::setApplicationProxy(new_proxy);
  }
}

AdBlockManager* WebFactory::adBlock() const {
  return m_adBlock;
}

#if defined(NO_LITE)
NetworkUrlInterceptor* WebFactory::urlIinterceptor() const {
  return m_urlInterceptor;
}

QWebEngineProfile* WebFactory::engineProfile() const {
  return m_engineProfile;
}

QAction* WebFactory::engineSettingsAction() {
  if (m_engineSettings == nullptr) {
    m_engineSettings =
      new QAction(qApp->icons()->fromTheme(QSL("applications-internet")), tr("Web engine settings"), this);
    m_engineSettings->setMenu(new QMenu());
    createMenu(m_engineSettings->menu());
    connect(m_engineSettings->menu(), &QMenu::aboutToShow, this, [this]() {
      createMenu();
    });
  }

  return m_engineSettings;
}

void WebFactory::createMenu(QMenu* menu) {
  if (menu == nullptr) {
    menu = qobject_cast<QMenu*>(sender());

    if (menu == nullptr) {
      return;
    }
  }

  menu->clear();
  QList<QAction*> actions;

  actions << createEngineSettingsAction(tr("Auto-load images"), QWebEngineSettings::WebAttribute::AutoLoadImages);
  actions << createEngineSettingsAction(tr("JS enabled"), QWebEngineSettings::WebAttribute::JavascriptEnabled);
  actions << createEngineSettingsAction(tr("JS can open popup windows"),
                                        QWebEngineSettings::WebAttribute::JavascriptCanOpenWindows);
  actions << createEngineSettingsAction(tr("JS can access clipboard"),
                                        QWebEngineSettings::WebAttribute::JavascriptCanAccessClipboard);
  actions << createEngineSettingsAction(tr("Hyperlinks can get focus"),
                                        QWebEngineSettings::WebAttribute::LinksIncludedInFocusChain);
  actions << createEngineSettingsAction(tr("Local storage enabled"),
                                        QWebEngineSettings::WebAttribute::LocalStorageEnabled);
  actions << createEngineSettingsAction(tr("Local content can access remote URLs"),
                                        QWebEngineSettings::WebAttribute::LocalContentCanAccessRemoteUrls);
  actions << createEngineSettingsAction(tr("XSS auditing enabled"),
                                        QWebEngineSettings::WebAttribute::XSSAuditingEnabled);
  actions << createEngineSettingsAction(tr("Spatial navigation enabled"),
                                        QWebEngineSettings::WebAttribute::SpatialNavigationEnabled);
  actions << createEngineSettingsAction(tr("Local content can access local files"),
                                        QWebEngineSettings::WebAttribute::LocalContentCanAccessFileUrls);
  actions << createEngineSettingsAction(tr("Hyperlink auditing enabled"),
                                        QWebEngineSettings::WebAttribute::HyperlinkAuditingEnabled);
  actions << createEngineSettingsAction(tr("Animate scrolling"),
                                        QWebEngineSettings::WebAttribute::ScrollAnimatorEnabled);
  actions << createEngineSettingsAction(tr("Error pages enabled"), QWebEngineSettings::WebAttribute::ErrorPageEnabled);
  actions << createEngineSettingsAction(tr("Plugins enabled"), QWebEngineSettings::WebAttribute::PluginsEnabled);
  actions << createEngineSettingsAction(tr("Fullscreen enabled"),
                                        QWebEngineSettings::WebAttribute::FullScreenSupportEnabled);

#if !defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
  actions << createEngineSettingsAction(tr("Screen capture enabled"),
                                        QWebEngineSettings::WebAttribute::ScreenCaptureEnabled);
  actions << createEngineSettingsAction(tr("WebGL enabled"), QWebEngineSettings::WebAttribute::WebGLEnabled);
  actions << createEngineSettingsAction(tr("Accelerate 2D canvas"),
                                        QWebEngineSettings::WebAttribute::Accelerated2dCanvasEnabled);
  actions << createEngineSettingsAction(tr("Print element backgrounds"),
                                        QWebEngineSettings::WebAttribute::PrintElementBackgrounds);
  actions << createEngineSettingsAction(tr("Allow running insecure content"),
                                        QWebEngineSettings::WebAttribute::AllowRunningInsecureContent);
  actions << createEngineSettingsAction(tr("Allow geolocation on insecure origins"),
                                        QWebEngineSettings::WebAttribute::AllowGeolocationOnInsecureOrigins);
#endif

  actions << createEngineSettingsAction(tr("JS can activate windows"),
                                        QWebEngineSettings::WebAttribute::AllowWindowActivationFromJavaScript);
  actions << createEngineSettingsAction(tr("Show scrollbars"), QWebEngineSettings::WebAttribute::ShowScrollBars);
  actions << createEngineSettingsAction(tr("Media playback with gestures"),
                                        QWebEngineSettings::WebAttribute::PlaybackRequiresUserGesture);
  actions << createEngineSettingsAction(tr("WebRTC uses only public interfaces"),
                                        QWebEngineSettings::WebAttribute::WebRTCPublicInterfacesOnly);
  actions << createEngineSettingsAction(tr("JS can paste from clipboard"),
                                        QWebEngineSettings::WebAttribute::JavascriptCanPaste);
  actions << createEngineSettingsAction(tr("DNS prefetch enabled"),
                                        QWebEngineSettings::WebAttribute::DnsPrefetchEnabled);

#if QT_VERSION >= 0x050D00 // Qt >= 5.13.0
  actions << createEngineSettingsAction(tr("PDF viewer enabled"), QWebEngineSettings::WebAttribute::PdfViewerEnabled);
#endif

  menu->addActions(actions);
}

void WebFactory::webEngineSettingChanged(bool enabled) {
  const QAction* const act = qobject_cast<QAction*>(sender());

  QWebEngineSettings::WebAttribute attribute = static_cast<QWebEngineSettings::WebAttribute>(act->data().toInt());

  qApp->settings()->setValue(WebEngineAttributes::ID, QString::number(static_cast<int>(attribute)), enabled);
  m_engineProfile->settings()->setAttribute(attribute, act->isChecked());
}

QAction* WebFactory::createEngineSettingsAction(const QString& title, int web_attribute) {
  // TODO: ověřit že cast je funkční
  QWebEngineSettings::WebAttribute attribute = QWebEngineSettings::WebAttribute(web_attribute);

  auto* act = new QAction(title, m_engineSettings->menu());

  act->setData(attribute);
  act->setCheckable(true);
  act->setChecked(qApp->settings()
                    ->value(WebEngineAttributes::ID, QString::number(static_cast<int>(attribute)), true)
                    .toBool());

  auto enabl = act->isChecked();

  m_engineProfile->settings()->setAttribute(attribute, enabl);
  connect(act, &QAction::toggled, this, &WebFactory::webEngineSettingChanged);
  return act;
}

#endif

CookieJar* WebFactory::cookieJar() const {
  return m_cookieJar;
}

Readability* WebFactory::readability() const {
  return m_readability;
}

ArticleParse* WebFactory::articleParse() const {
  return m_articleParse;
}

void WebFactory::startApiServer() {
  m_apiServer = new ApiServer(this);
  m_apiServer->setListenAddressPort(QSL("http://localhost:54123"), true);

  qDebugNN << LOGSEC_NETWORK << "Started API server:" << QUOTE_W_SPACE_DOT(m_apiServer->listenAddressPort());
}

void WebFactory::stopApiServer() {
  if (m_apiServer != nullptr) {
    qDebugNN << LOGSEC_NETWORK << "Stopped API server:" << QUOTE_W_SPACE_DOT(m_apiServer->listenAddressPort());

    delete m_apiServer;
    m_apiServer = nullptr;
  }
}

QMap<QString, char16_t> WebFactory::generateUnescapes() {
  QMap<QString, char16_t> res;
  res[QSL("AElig")] = 0x00c6;
  res[QSL("AMP")] = 38;
  res[QSL("Aacute")] = 0x00c1;
  res[QSL("Acirc")] = 0x00c2;
  res[QSL("Agrave")] = 0x00c0;
  res[QSL("Alpha")] = 0x0391;
  res[QSL("Aring")] = 0x00c5;
  res[QSL("Atilde")] = 0x00c3;
  res[QSL("Auml")] = 0x00c4;
  res[QSL("Beta")] = 0x0392;
  res[QSL("Ccedil")] = 0x00c7;
  res[QSL("Chi")] = 0x03a7;
  res[QSL("Dagger")] = 0x2021;
  res[QSL("Delta")] = 0x0394;
  res[QSL("ETH")] = 0x00d0;
  res[QSL("Eacute")] = 0x00c9;
  res[QSL("Ecirc")] = 0x00ca;
  res[QSL("Egrave")] = 0x00c8;
  res[QSL("Epsilon")] = 0x0395;
  res[QSL("Eta")] = 0x0397;
  res[QSL("Euml")] = 0x00cb;
  res[QSL("GT")] = 62;
  res[QSL("Gamma")] = 0x0393;
  res[QSL("Iacute")] = 0x00cd;
  res[QSL("Icirc")] = 0x00ce;
  res[QSL("Igrave")] = 0x00cc;
  res[QSL("Iota")] = 0x0399;
  res[QSL("Iuml")] = 0x00cf;
  res[QSL("Kappa")] = 0x039a;
  res[QSL("LT")] = 60;
  res[QSL("Lambda")] = 0x039b;
  res[QSL("Mu")] = 0x039c;
  res[QSL("Ntilde")] = 0x00d1;
  res[QSL("Nu")] = 0x039d;
  res[QSL("OElig")] = 0x0152;
  res[QSL("Oacute")] = 0x00d3;
  res[QSL("Ocirc")] = 0x00d4;
  res[QSL("Ograve")] = 0x00d2;
  res[QSL("Omega")] = 0x03a9;
  res[QSL("Omicron")] = 0x039f;
  res[QSL("Oslash")] = 0x00d8;
  res[QSL("Otilde")] = 0x00d5;
  res[QSL("Ouml")] = 0x00d6;
  res[QSL("Phi")] = 0x03a6;
  res[QSL("Pi")] = 0x03a0;
  res[QSL("Prime")] = 0x2033;
  res[QSL("Psi")] = 0x03a8;
  res[QSL("QUOT")] = 34;
  res[QSL("Rho")] = 0x03a1;
  res[QSL("Scaron")] = 0x0160;
  res[QSL("Sigma")] = 0x03a3;
  res[QSL("THORN")] = 0x00de;
  res[QSL("Tau")] = 0x03a4;
  res[QSL("Theta")] = 0x0398;
  res[QSL("Uacute")] = 0x00da;
  res[QSL("Ucirc")] = 0x00db;
  res[QSL("Ugrave")] = 0x00d9;
  res[QSL("Upsilon")] = 0x03a5;
  res[QSL("Uuml")] = 0x00dc;
  res[QSL("Xi")] = 0x039e;
  res[QSL("Yacute")] = 0x00dd;
  res[QSL("Yuml")] = 0x0178;
  res[QSL("Zeta")] = 0x0396;
  res[QSL("aacute")] = 0x00e1;
  res[QSL("acirc")] = 0x00e2;
  res[QSL("acute")] = 0x00b4;
  res[QSL("aelig")] = 0x00e6;
  res[QSL("agrave")] = 0x00e0;
  res[QSL("alefsym")] = 0x2135;
  res[QSL("alpha")] = 0x03b1;
  res[QSL("amp")] = 38;
  res[QSL("and")] = 0x22a5;
  res[QSL("ang")] = 0x2220;
  res[QSL("apos")] = 0x0027;
  res[QSL("aring")] = 0x00e5;
  res[QSL("asymp")] = 0x2248;
  res[QSL("atilde")] = 0x00e3;
  res[QSL("auml")] = 0x00e4;
  res[QSL("bdquo")] = 0x201e;
  res[QSL("beta")] = 0x03b2;
  res[QSL("brvbar")] = 0x00a6;
  res[QSL("bull")] = 0x2022;
  res[QSL("cap")] = 0x2229;
  res[QSL("ccedil")] = 0x00e7;
  res[QSL("cedil")] = 0x00b8;
  res[QSL("cent")] = 0x00a2;
  res[QSL("chi")] = 0x03c7;
  res[QSL("circ")] = 0x02c6;
  res[QSL("clubs")] = 0x2663;
  res[QSL("cong")] = 0x2245;
  res[QSL("copy")] = 0x00a9;
  res[QSL("crarr")] = 0x21b5;
  res[QSL("cup")] = 0x222a;
  res[QSL("curren")] = 0x00a4;
  res[QSL("dArr")] = 0x21d3;
  res[QSL("dagger")] = 0x2020;
  res[QSL("darr")] = 0x2193;
  res[QSL("deg")] = 0x00b0;
  res[QSL("delta")] = 0x03b4;
  res[QSL("diams")] = 0x2666;
  res[QSL("divide")] = 0x00f7;
  res[QSL("eacute")] = 0x00e9;
  res[QSL("ecirc")] = 0x00ea;
  res[QSL("egrave")] = 0x00e8;
  res[QSL("empty")] = 0x2205;
  res[QSL("emsp")] = 0x2003;
  res[QSL("ensp")] = 0x2002;
  res[QSL("epsilon")] = 0x03b5;
  res[QSL("equiv")] = 0x2261;
  res[QSL("eta")] = 0x03b7;
  res[QSL("eth")] = 0x00f0;
  res[QSL("euml")] = 0x00eb;
  res[QSL("euro")] = 0x20ac;
  res[QSL("exist")] = 0x2203;
  res[QSL("fnof")] = 0x0192;
  res[QSL("forall")] = 0x2200;
  res[QSL("frac12")] = 0x00bd;
  res[QSL("frac14")] = 0x00bc;
  res[QSL("frac34")] = 0x00be;
  res[QSL("frasl")] = 0x2044;
  res[QSL("gamma")] = 0x03b3;
  res[QSL("ge")] = 0x2265;
  res[QSL("gt")] = 62;
  res[QSL("hArr")] = 0x21d4;
  res[QSL("harr")] = 0x2194;
  res[QSL("hearts")] = 0x2665;
  res[QSL("hellip")] = 0x2026;
  res[QSL("iacute")] = 0x00ed;
  res[QSL("icirc")] = 0x00ee;
  res[QSL("iexcl")] = 0x00a1;
  res[QSL("igrave")] = 0x00ec;
  res[QSL("image")] = 0x2111;
  res[QSL("infin")] = 0x221e;
  res[QSL("int")] = 0x222b;
  res[QSL("iota")] = 0x03b9;
  res[QSL("iquest")] = 0x00bf;
  res[QSL("isin")] = 0x2208;
  res[QSL("iuml")] = 0x00ef;
  res[QSL("kappa")] = 0x03ba;
  res[QSL("lArr")] = 0x21d0;
  res[QSL("lambda")] = 0x03bb;
  res[QSL("lang")] = 0x2329;
  res[QSL("laquo")] = 0x00ab;
  res[QSL("larr")] = 0x2190;
  res[QSL("lceil")] = 0x2308;
  res[QSL("ldquo")] = 0x201c;
  res[QSL("le")] = 0x2264;
  res[QSL("lfloor")] = 0x230a;
  res[QSL("lowast")] = 0x2217;
  res[QSL("loz")] = 0x25ca;
  res[QSL("lrm")] = 0x200e;
  res[QSL("lsaquo")] = 0x2039;
  res[QSL("lsquo")] = 0x2018;
  res[QSL("lt")] = 60;
  res[QSL("macr")] = 0x00af;
  res[QSL("mdash")] = 0x2014;
  res[QSL("micro")] = 0x00b5;
  res[QSL("middot")] = 0x00b7;
  res[QSL("minus")] = 0x2212;
  res[QSL("mu")] = 0x03bc;
  res[QSL("nabla")] = 0x2207;
  res[QSL("nbsp")] = 0x00a0;
  res[QSL("ndash")] = 0x2013;
  res[QSL("ne")] = 0x2260;
  res[QSL("ni")] = 0x220b;
  res[QSL("not")] = 0x00ac;
  res[QSL("notin")] = 0x2209;
  res[QSL("nsub")] = 0x2284;
  res[QSL("ntilde")] = 0x00f1;
  res[QSL("nu")] = 0x03bd;
  res[QSL("oacute")] = 0x00f3;
  res[QSL("ocirc")] = 0x00f4;
  res[QSL("oelig")] = 0x0153;
  res[QSL("ograve")] = 0x00f2;
  res[QSL("oline")] = 0x203e;
  res[QSL("omega")] = 0x03c9;
  res[QSL("omicron")] = 0x03bf;
  res[QSL("oplus")] = 0x2295;
  res[QSL("or")] = 0x22a6;
  res[QSL("ordf")] = 0x00aa;
  res[QSL("ordm")] = 0x00ba;
  res[QSL("oslash")] = 0x00f8;
  res[QSL("otilde")] = 0x00f5;
  res[QSL("otimes")] = 0x2297;
  res[QSL("ouml")] = 0x00f6;
  res[QSL("para")] = 0x00b6;
  res[QSL("part")] = 0x2202;
  res[QSL("percnt")] = 0x0025;
  res[QSL("permil")] = 0x2030;
  res[QSL("perp")] = 0x22a5;
  res[QSL("phi")] = 0x03c6;
  res[QSL("pi")] = 0x03c0;
  res[QSL("piv")] = 0x03d6;
  res[QSL("plusmn")] = 0x00b1;
  res[QSL("pound")] = 0x00a3;
  res[QSL("prime")] = 0x2032;
  res[QSL("prod")] = 0x220f;
  res[QSL("prop")] = 0x221d;
  res[QSL("psi")] = 0x03c8;
  res[QSL("quot")] = 34;
  res[QSL("rArr")] = 0x21d2;
  res[QSL("radic")] = 0x221a;
  res[QSL("rang")] = 0x232a;
  res[QSL("raquo")] = 0x00bb;
  res[QSL("rarr")] = 0x2192;
  res[QSL("rceil")] = 0x2309;
  res[QSL("rdquo")] = 0x201d;
  res[QSL("real")] = 0x211c;
  res[QSL("reg")] = 0x00ae;
  res[QSL("rfloor")] = 0x230b;
  res[QSL("rho")] = 0x03c1;
  res[QSL("rlm")] = 0x200f;
  res[QSL("rsaquo")] = 0x203a;
  res[QSL("rsquo")] = 0x2019;
  res[QSL("sbquo")] = 0x201a;
  res[QSL("scaron")] = 0x0161;
  res[QSL("sdot")] = 0x22c5;
  res[QSL("sect")] = 0x00a7;
  res[QSL("shy")] = 0x00ad;
  res[QSL("sigma")] = 0x03c3;
  res[QSL("sigmaf")] = 0x03c2;
  res[QSL("sim")] = 0x223c;
  res[QSL("spades")] = 0x2660;
  res[QSL("sub")] = 0x2282;
  res[QSL("sube")] = 0x2286;
  res[QSL("sum")] = 0x2211;
  res[QSL("sup")] = 0x2283;
  res[QSL("sup1")] = 0x00b9;
  res[QSL("sup2")] = 0x00b2;
  res[QSL("sup3")] = 0x00b3;
  res[QSL("supe")] = 0x2287;
  res[QSL("szlig")] = 0x00df;
  res[QSL("tau")] = 0x03c4;
  res[QSL("there4")] = 0x2234;
  res[QSL("theta")] = 0x03b8;
  res[QSL("thetasym")] = 0x03d1;
  res[QSL("thinsp")] = 0x2009;
  res[QSL("thorn")] = 0x00fe;
  res[QSL("tilde")] = 0x02dc;
  res[QSL("times")] = 0x00d7;
  res[QSL("trade")] = 0x2122;
  res[QSL("uArr")] = 0x21d1;
  res[QSL("uacute")] = 0x00fa;
  res[QSL("uarr")] = 0x2191;
  res[QSL("ucirc")] = 0x00fb;
  res[QSL("ugrave")] = 0x00f9;
  res[QSL("uml")] = 0x00a8;
  res[QSL("upsih")] = 0x03d2;
  res[QSL("upsilon")] = 0x03c5;
  res[QSL("uuml")] = 0x00fc;
  res[QSL("weierp")] = 0x2118;
  res[QSL("xi")] = 0x03be;
  res[QSL("yacute")] = 0x00fd;
  res[QSL("yen")] = 0x00a5;
  res[QSL("yuml")] = 0x00ff;
  res[QSL("zeta")] = 0x03b6;
  res[QSL("zwj")] = 0x200d;
  res[QSL("zwnj")] = 0x200c;

  return res;
}

QString WebFactory::customUserAgent() const {
  return m_customUserAgent;
}

void WebFactory::setCustomUserAgent(const QString& user_agent) {
  m_customUserAgent = user_agent;
}

#if defined(NO_LITE)
void WebFactory::cleanupCache() {
  if (MsgBox::show(nullptr,
                   QMessageBox::Icon::Question,
                   tr("Web cache is going to be cleared"),
                   tr("Do you really want to clear web cache?"),
                   {},
                   {},
                   QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No) ==
      QMessageBox::StandardButton::Yes) {
    m_engineProfile->clearHttpCache();

    // NOTE: Manually clear storage.
    try {
      IOFactory::removeFolder(m_engineProfile->persistentStoragePath());
    }
    catch (const ApplicationException& ex) {
      qCriticalNN << LOGSEC_CORE << "Removing of webengine storage failed:" << QUOTE_W_SPACE_DOT(ex.message());
    }
  }
}
#endif
