// For license of this file, see <project-root-folder>/LICENSE.md.

#include "services/gmail/gui/emailpreviewer.h"

#include "exceptions/networkexception.h"
#include "gui/messagebox.h"
#include "miscellaneous/application.h"
#include "miscellaneous/iconfactory.h"
#include "network-web/oauth2service.h"
#include "services/gmail/definitions.h"
#include "services/gmail/gmailnetworkfactory.h"
#include "services/gmail/gmailserviceroot.h"
#include "services/gmail/gui/formaddeditemail.h"

#include <QJsonObject>

EmailPreviewer::EmailPreviewer(GmailServiceRoot* account, QWidget* parent)
  : CustomMessagePreviewer(parent), m_account(account), m_webView(new WebBrowser(nullptr, this)) {
  m_ui.setupUi(this);

  m_ui.m_mainLayout->addWidget(dynamic_cast<QWidget*>(m_webView.data()), 3, 0, 1, -1);
  m_ui.m_btnAttachments->setIcon(qApp->icons()->fromTheme(QSL("mail-attachment")));
  m_ui.m_btnForward->setIcon(qApp->icons()->fromTheme(QSL("mail-forward")));
  m_ui.m_btnReply->setIcon(qApp->icons()->fromTheme(QSL("mail-reply-sender")));

  QMenu* menu_attachments = new QMenu(this);

  m_ui.m_btnAttachments->setMenu(menu_attachments);

  m_webView->setNavigationBarVisible(false);

  connect(menu_attachments, &QMenu::triggered, this, &EmailPreviewer::downloadAttachment);
  connect(m_ui.m_btnReply, &QToolButton::clicked, this, &EmailPreviewer::replyToEmail);
  connect(m_ui.m_btnForward, &QToolButton::clicked, this, &EmailPreviewer::forwardEmail);
}

EmailPreviewer::~EmailPreviewer() {
  qDebugNN << LOGSEC_GMAIL << "Email previewer destroyed.";
}

void EmailPreviewer::clear() {
  m_webView->clear(false);
}

void EmailPreviewer::loadMessage(const Message& msg, RootItem* selected_item) {
  Q_UNUSED(selected_item)

  m_message = msg;
  m_webView->setHtml(msg.m_contents);

  m_ui.m_tbFrom->setText(msg.m_author);
  m_ui.m_tbSubject->setText(msg.m_title);
  m_ui.m_tbTo->setText(QSL("-"));

  m_ui.m_btnAttachments->menu()->clear();

  for (const Enclosure& att : msg.m_enclosures) {
    const QStringList att_id_name = att.m_url.split(QSL(GMAIL_ATTACHMENT_SEP));

    m_ui.m_btnAttachments->menu()->addAction(att.m_mimeType)->setData(att_id_name);
  }

  m_ui.m_btnAttachments->setDisabled(m_ui.m_btnAttachments->menu()->isEmpty());
}

void EmailPreviewer::replyToEmail() {
  FormAddEditEmail(m_account, window()).execForReply(&m_message);
}

void EmailPreviewer::forwardEmail() {
  FormAddEditEmail(m_account, window()).execForForward(&m_message);
}

void EmailPreviewer::downloadAttachment(QAction* act) {
  const QString attachment_id = act->data().toStringList().at(1);
  const QString file_name = act->data().toStringList().at(0);

  try {

    const QNetworkRequest req = m_account->network()->requestForAttachment(m_message.m_customId, attachment_id);

    qApp->downloadManager()->download(req, file_name, [this](DownloadItem* it) {
      if (it->downloadedSuccessfully()) {
        const QByteArray raw_json = IOFactory::readFile(it->output().fileName());
        const QString data = QJsonDocument::fromJson(raw_json).object()[QSL("data")].toString();

        if (!data.isEmpty()) {
          IOFactory::writeFile(it->output().fileName(),
                               QByteArray::fromBase64(data.toLocal8Bit(),
                                                      QByteArray::Base64Option::Base64UrlEncoding));
        }
      }
    });
  }
  catch (const NetworkException&) {
    MsgBox::show({},
                 QMessageBox::Icon::Critical,
                 tr("Cannot download attachment"),
                 tr("Attachment cannot be downloaded because you are not logged-in."));
  }
  catch (const ApplicationException& ex) {
    MsgBox::show({},
                 QMessageBox::Icon::Critical,
                 tr("Cannot download attachment"),
                 tr("Attachment cannot be downloaded because some general error happened."),
                 {},
                 ex.message());
  }
}
