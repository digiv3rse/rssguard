// For license of this file, see <project-root-folder>/LICENSE.md.

#include "core/message.h"

#include "3rd-party/boolinq/boolinq.h"
#include "miscellaneous/textfactory.h"
#include "services/abstract/feed.h"
#include "services/abstract/label.h"

#include <QDebug>
#include <QFlags>
#include <QRegularExpression>
#include <QUrl>
#include <QVariant>
#include <QVector>

Enclosure::Enclosure(QString url, QString mime) : m_url(std::move(url)), m_mimeType(std::move(mime)) {}

QList<Enclosure> Enclosures::decodeEnclosuresFromString(const QString& enclosures_data) {
  QList<Enclosure> enclosures;
  auto enc = enclosures_data.split(ENCLOSURES_OUTER_SEPARATOR,
#if QT_VERSION >= 0x050F00 // Qt >= 5.15.0
                                   Qt::SplitBehaviorFlags::SkipEmptyParts);
#else
                                   QString::SplitBehavior::SkipEmptyParts);
#endif

  for (const QString& single_enclosure : qAsConst(enc)) {
    Enclosure enclosure;

    if (single_enclosure.contains(ECNLOSURES_INNER_SEPARATOR)) {
      QStringList mime_url = single_enclosure.split(ECNLOSURES_INNER_SEPARATOR);

      enclosure.m_mimeType = QByteArray::fromBase64(mime_url.at(0).toLocal8Bit());
      enclosure.m_url = QByteArray::fromBase64(mime_url.at(1).toLocal8Bit());
    }
    else {
      enclosure.m_url = QByteArray::fromBase64(single_enclosure.toLocal8Bit());
    }

    enclosures.append(enclosure);
  }

  return enclosures;
}

QString Enclosures::encodeEnclosuresToString(const QList<Enclosure>& enclosures) {
  QStringList enclosures_str;

  for (const Enclosure& enclosure : enclosures) {
    if (enclosure.m_mimeType.isEmpty()) {
      enclosures_str.append(enclosure.m_url.toLocal8Bit().toBase64());
    }
    else {
      enclosures_str.append(QString(enclosure.m_mimeType.toLocal8Bit().toBase64()) +
                            ECNLOSURES_INNER_SEPARATOR +
                            enclosure.m_url.toLocal8Bit().toBase64());
    }
  }

  return enclosures_str.join(QString(ENCLOSURES_OUTER_SEPARATOR));
}

Message::Message() {
  m_title = m_url = m_author = m_contents = m_rawContents = m_feedId = m_customId = m_customHash = QL1S("");
  m_enclosures = QList<Enclosure>();
  m_accountId = m_id = 0;
  m_score = 0.0;
  m_isRead = m_isImportant = m_isDeleted = false;
  m_assignedLabels = QList<Label*>();
}

void Message::sanitize(const Feed* feed, bool fix_future_datetimes) {
  // Sanitize title.
  m_title = m_title

            // Remove non-breaking spaces.
            .replace(QRegularExpression(QString::fromUtf8(QByteArray("[\xE2\x80\xAF]"))), QSL(" "))

            // Shrink consecutive whitespaces.
            .replace(QRegularExpression(QSL("[\\s]{2,}")), QSL(" "))

            // Remove all newlines and leading white space.
            .remove(QRegularExpression(QSL("([\\n\\r])|(^\\s)")));

  // Check if messages contain relative URLs and if they do, then replace them.
  if (m_url.startsWith(QL1S("//"))) {
    m_url = QSL(URI_SCHEME_HTTPS) + m_url.mid(2);
  }
  else if (QUrl(m_url).isRelative()) {
    QUrl base(feed->source());

    if (base.isValid()) {
      base = QUrl(base.scheme() + QSL("://") + base.host());

      m_url = base.resolved(m_url).toString();
    }
  }

  // Fix datetimes in future.
  if ((fix_future_datetimes && m_createdFromFeed && m_created.toUTC() > QDateTime::currentDateTimeUtc()) ||
      (m_createdFromFeed && !m_created.isValid())) {
    qWarningNN << LOGSEC_CORE << "Fixing date of article" << QUOTE_W_SPACE(m_title) << "from invalid date/time"
               << QUOTE_W_SPACE_DOT(m_created);

    m_createdFromFeed = false;
    m_created = QDateTime::currentDateTimeUtc();
  }
}

Message Message::fromSqlRecord(const QSqlRecord& record, bool* result) {
  if (record.count() != MSG_DB_HAS_ENCLOSURES + 1) {
    if (result != nullptr) {
      *result = false;
    }

    return Message();
  }

  Message message;

  message.m_id = record.value(MSG_DB_ID_INDEX).toInt();
  message.m_isRead = record.value(MSG_DB_READ_INDEX).toBool();
  message.m_isImportant = record.value(MSG_DB_IMPORTANT_INDEX).toBool();
  message.m_isDeleted = record.value(MSG_DB_DELETED_INDEX).toBool();
  message.m_feedId = record.value(MSG_DB_FEED_CUSTOM_ID_INDEX).toString();
  message.m_title = record.value(MSG_DB_TITLE_INDEX).toString();
  message.m_url = record.value(MSG_DB_URL_INDEX).toString();
  message.m_author = record.value(MSG_DB_AUTHOR_INDEX).toString();
  message.m_created = TextFactory::parseDateTime(record.value(MSG_DB_DCREATED_INDEX).value<qint64>());
  message.m_contents = record.value(MSG_DB_CONTENTS_INDEX).toString();
  message.m_enclosures = Enclosures::decodeEnclosuresFromString(record.value(MSG_DB_ENCLOSURES_INDEX).toString());
  message.m_score = record.value(MSG_DB_SCORE_INDEX).toDouble();
  message.m_accountId = record.value(MSG_DB_ACCOUNT_ID_INDEX).toInt();
  message.m_customId = record.value(MSG_DB_CUSTOM_ID_INDEX).toString();
  message.m_customHash = record.value(MSG_DB_CUSTOM_HASH_INDEX).toString();

  if (result != nullptr) {
    *result = true;
  }

  return message;
}

QString Message::generateRawAtomContents(const Message& msg) {
  return QSL("<entry>"
             "<title>%1</title>"
             "<link href=\"%2\" rel=\"alternate\" type=\"text/html\" title=\"%1\"/>"
             "<published>%3</published>"
             "<author><name>%6</name></author>"
             "<updated>%3</updated>"
             "<id>%4</id>"
             "<summary type=\"html\">%5</summary>"
             "</entry>").arg(msg.m_title,
                             msg.m_url,
                             msg.m_created.toUTC().toString(QSL("yyyy-MM-ddThh:mm:ss")),
                             msg.m_url,
                             msg.m_contents.toHtmlEscaped(),
                             msg.m_author);
}

QDataStream& operator<<(QDataStream& out, const Message& my_obj) {
  out << my_obj.m_accountId
      << my_obj.m_customHash
      << my_obj.m_customId
      << my_obj.m_feedId
      << my_obj.m_id
      << my_obj.m_isImportant
      << my_obj.m_isRead
      << my_obj.m_isDeleted
      << my_obj.m_score;

  return out;
}

QDataStream& operator>>(QDataStream& in, Message& my_obj) {
  int account_id;
  QString custom_hash;
  QString custom_id;
  QString feed_id;
  int id;
  bool is_important;
  bool is_read;
  bool is_deleted;
  double score;

  in >> account_id >> custom_hash >> custom_id >> feed_id >> id >> is_important >> is_read >> is_deleted >> score;

  my_obj.m_accountId = account_id;
  my_obj.m_customHash = custom_hash;
  my_obj.m_customId = custom_id;
  my_obj.m_feedId = feed_id;
  my_obj.m_id = id;
  my_obj.m_isImportant = is_important;
  my_obj.m_isRead = is_read;
  my_obj.m_isDeleted = is_deleted;
  my_obj.m_score = score;

  return in;
}

uint qHash(const Message& key, uint seed) {
  Q_UNUSED(seed)
  return (uint(key.m_accountId) * 10000) + uint(key.m_id);
}

uint qHash(const Message& key) {
  return (uint(key.m_accountId) * 10000) + uint(key.m_id);
}
