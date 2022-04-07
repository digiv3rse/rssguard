// For license of this file, see <project-root-folder>/LICENSE.md.

#include "gui/messagepreviewer.h"

#include "database/databasequeries.h"
#include "gui/dialogs/formmain.h"
#include "gui/messagebox.h"
#include "gui/reusable/plaintoolbutton.h"
#include "gui/reusable/searchtextwidget.h"
#include "gui/webbrowser.h"
#include "miscellaneous/application.h"
#include "network-web/webfactory.h"
#include "services/abstract/gui/custommessagepreviewer.h"
#include "services/abstract/label.h"
#include "services/abstract/labelsnode.h"
#include "services/abstract/serviceroot.h"
#include "services/gmail/gui/emailpreviewer.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStackedLayout>
#include <QToolBar>
#include <QToolTip>

void MessagePreviewer::createConnections() {
  installEventFilter(this);

  connect(m_actionMarkRead = m_toolBar->addAction(qApp->icons()->fromTheme(QSL("mail-mark-read")), tr("Mark article read")),
          &QAction::triggered,
          this,
          &MessagePreviewer::markMessageAsRead);
  connect(m_actionMarkUnread = m_toolBar->addAction(qApp->icons()->fromTheme(QSL("mail-mark-unread")), tr("Mark article unread")),
          &QAction::triggered,
          this,
          &MessagePreviewer::markMessageAsUnread);
  connect(m_actionSwitchImportance = m_toolBar->addAction(qApp->icons()->fromTheme(QSL("mail-mark-important")), tr("Switch article importance")),
          &QAction::triggered,
          this,
          &MessagePreviewer::switchMessageImportance);
}

MessagePreviewer::MessagePreviewer(QWidget* parent)
  : QWidget(parent), m_mainLayout(new QGridLayout(this)), m_viewerLayout(new QStackedLayout(this)),
  m_toolBar(new QToolBar(this)), m_msgBrowser(new WebBrowser(nullptr, this)), m_separator(nullptr),
  m_btnLabels(QList<QPair<LabelButton*, QAction*>>()) {

  m_toolBar->setOrientation(Qt::Orientation::Vertical);

  // NOTE: To make sure that if we have many labels and short message
  // that whole toolbar is visible.
  m_toolBar->setSizePolicy(m_toolBar->sizePolicy().horizontalPolicy(), QSizePolicy::Policy::MinimumExpanding);

  // This layout holds standard article browser on index 0
  // and optional custom browser on index 1.
  m_viewerLayout->addWidget(m_msgBrowser);

  m_mainLayout->setContentsMargins(3, 3, 3, 3);
  m_mainLayout->addLayout(m_viewerLayout, 0, 1, 1, 1);
  m_mainLayout->addWidget(m_toolBar, 0, 0, -1, 1);

  createConnections();

  m_actionSwitchImportance->setCheckable(true);

  clear();
}

MessagePreviewer::~MessagePreviewer() {
  if (m_viewerLayout->count() > 1) {
    // Make sure that previewer does not delete any custom article
    // viewers as those are responsibility to free by their accounts.
    auto* wdg = m_viewerLayout->widget(1);

    wdg->setParent(nullptr);

    m_viewerLayout->removeWidget(wdg);
  }
}

void MessagePreviewer::reloadFontSettings() {
  m_msgBrowser->reloadFontSettings();
}

void MessagePreviewer::setToolbarsVisible(bool visible) {
  m_toolBar->setVisible(visible);
  m_msgBrowser->setNavigationBarVisible(visible);

  qApp->settings()->setValue(GROUP(GUI), GUI::MessageViewerToolbarsVisible, visible);
}

WebBrowser* MessagePreviewer::webBrowser() const {
  return m_msgBrowser;
}

void MessagePreviewer::clear() {
  updateLabels(true);
  ensureDefaultBrowserVisible();
  m_msgBrowser->clear(false);
  hide();
  m_root.clear();
  m_message = Message();
}

void MessagePreviewer::hideToolbar() {
  m_toolBar->setVisible(false);
}

void MessagePreviewer::loadUrl(const QString& url) {
  ensureDefaultBrowserVisible();
  m_msgBrowser->loadUrl(url);
}

void MessagePreviewer::loadMessage(const Message& message, RootItem* root) {
  bool same_message = message.m_id == m_message.m_id && m_root == root;

  m_message = message;
  m_root = root;

  if (!m_root.isNull()) {
    updateButtons();
    updateLabels(false);
    show();

    if (!same_message) {
      const QString msg_feed_id = message.m_feedId;
      const auto* feed = root->getParentServiceRoot()->getItemFromSubTree(
        [msg_feed_id](const RootItem* it) {
        return it->kind() == RootItem::Kind::Feed && it->customId() == msg_feed_id;
      })->toFeed();

      if (feed != nullptr && feed->openArticlesDirectly() && !m_message.m_url.isEmpty()) {
        ensureDefaultBrowserVisible();

        m_msgBrowser->setVerticalScrollBarPosition(0.0);
        m_msgBrowser->loadUrl(m_message.m_url);
      }
      else {
        CustomMessagePreviewer* custom_previewer = root->getParentServiceRoot()->customMessagePreviewer();

        if (custom_previewer != nullptr) {
          auto* current_custom_previewer = m_viewerLayout->widget(1);

          if (current_custom_previewer != nullptr) {
            if (current_custom_previewer != custom_previewer) {
              m_viewerLayout->removeWidget(current_custom_previewer);
              m_viewerLayout->addWidget(custom_previewer);
            }
          }
          else {
            m_viewerLayout->addWidget(custom_previewer);
          }

          m_viewerLayout->setCurrentIndex(1);
          custom_previewer->loadMessage(message, root);
        }
        else {
          ensureDefaultBrowserVisible();

          m_msgBrowser->loadMessages({ message }, m_root);
        }
      }
    }
  }
}

void MessagePreviewer::switchLabel(bool assign) {
  auto lbl = qobject_cast<LabelButton*>(sender())->label();

  if (lbl == nullptr) {
    return;
  }

  if (assign) {
    lbl->assignToMessage(m_message);
  }
  else {
    lbl->deassignFromMessage(m_message);
  }
}

void MessagePreviewer::markMessageAsRead() {
  markMessageAsReadUnread(RootItem::ReadStatus::Read);
}

void MessagePreviewer::markMessageAsUnread() {
  markMessageAsReadUnread(RootItem::ReadStatus::Unread);
}

void MessagePreviewer::markMessageAsReadUnread(RootItem::ReadStatus read) {
  if (!m_root.isNull()) {
    if (m_root->getParentServiceRoot()->onBeforeSetMessagesRead(m_root.data(),
                                                                QList<Message>() << m_message,
                                                                read)) {
      DatabaseQueries::markMessagesReadUnread(qApp->database()->driver()->connection(objectName(),
                                                                                     DatabaseDriver::DesiredStorageType::FromSettings),
                                              QStringList() << QString::number(m_message.m_id),
                                              read);
      m_root->getParentServiceRoot()->onAfterSetMessagesRead(m_root.data(),
                                                             QList<Message>() << m_message,
                                                             read);
      m_message.m_isRead = read == RootItem::ReadStatus::Read;
      emit markMessageRead(m_message.m_id, read);

      updateButtons();
    }
  }
}

void MessagePreviewer::switchMessageImportance(bool checked) {
  if (!m_root.isNull()) {
    if (m_root->getParentServiceRoot()->onBeforeSwitchMessageImportance(m_root.data(),
                                                                        QList<ImportanceChange>()
                                                                        << ImportanceChange(m_message,
                                                                                            m_message.
                                                                                            m_isImportant
                                                                                            ? RootItem::Importance::NotImportant
                                                                                            : RootItem::Importance::Important))) {
      DatabaseQueries::switchMessagesImportance(qApp->database()->driver()->connection(objectName(), DatabaseDriver::DesiredStorageType::FromSettings),
                                                QStringList() << QString::number(m_message.m_id));
      m_root->getParentServiceRoot()->onAfterSwitchMessageImportance(m_root.data(),
                                                                     QList<ImportanceChange>()
                                                                     << ImportanceChange(m_message,
                                                                                         m_message.m_isImportant
                                                                                         ? RootItem::Importance::NotImportant
                                                                                         : RootItem::Importance::Important));
      emit markMessageImportant(m_message.m_id, checked
                                ? RootItem::Importance::Important
                                : RootItem::Importance::NotImportant);

      m_message.m_isImportant = checked;
    }
  }
}

void MessagePreviewer::updateButtons() {
  m_actionSwitchImportance->setChecked(m_message.m_isImportant);
  m_actionMarkRead->setEnabled(!m_message.m_isRead);
  m_actionMarkUnread->setEnabled(m_message.m_isRead);
}

void MessagePreviewer::updateLabels(bool only_clear) {
  for (auto& lbl : m_btnLabels) {
    m_toolBar->removeAction(lbl.second);
    lbl.second->deleteLater();
    lbl.first->deleteLater();
  }

  m_btnLabels.clear();

  if (m_separator != nullptr) {
    m_toolBar->removeAction(m_separator);
  }

  if (only_clear) {
    return;
  }

  if (m_root.data() != nullptr && !m_root.data()->getParentServiceRoot()->labelsNode()->labels().isEmpty()) {
    m_separator = m_toolBar->addSeparator();
    QSqlDatabase database = qApp->database()->driver()->connection(metaObject()->className());
    auto lbls = m_root.data()->getParentServiceRoot()->labelsNode()->labels();

    for (auto* label : lbls) {
      LabelButton* btn_label = new LabelButton(this);

      btn_label->setLabel(label);
      btn_label->setCheckable(true);
      btn_label->setIcon(Label::generateIcon(label->color()));
      btn_label->setAutoRaise(false);
      btn_label->setText(QSL(" ") + label->title());
      btn_label->setToolButtonStyle(Qt::ToolButtonStyle(qApp->settings()->value(GROUP(GUI),
                                                                                SETTING(GUI::ToolbarStyle)).toInt()));
      btn_label->setToolTip(label->title());
      btn_label->setChecked(DatabaseQueries::isLabelAssignedToMessage(database, label, m_message));

      QAction* act_label = m_toolBar->addWidget(btn_label);

      connect(btn_label, &QToolButton::toggled, this, &MessagePreviewer::switchLabel);

      m_btnLabels.append({ btn_label, act_label });
    }
  }
}

void MessagePreviewer::ensureDefaultBrowserVisible() {
  if (m_viewerLayout->count() > 1) {
    m_viewerLayout->removeWidget(m_viewerLayout->widget(1));
  }

  m_viewerLayout->setCurrentIndex(0);
}

LabelButton::LabelButton(QWidget* parent) : QToolButton(parent), m_label(nullptr) {}

Label* LabelButton::label() const {
  return m_label.data();
}

void LabelButton::setLabel(Label* label) {
  m_label = label;
}
