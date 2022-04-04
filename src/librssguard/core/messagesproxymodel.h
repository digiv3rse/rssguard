// For license of this file, see <project-root-folder>/LICENSE.md.

#ifndef MESSAGESPROXYMODEL_H
#define MESSAGESPROXYMODEL_H

#include <QSortFilterProxyModel>

class MessagesModel;
class Message;

class MessagesProxyModel : public QSortFilterProxyModel {
  Q_OBJECT

  public:
    // Enum which describes basic filtering schemes
    // for messages.
    enum class MessageFilter {
      NoFiltering = 100,
      FilterUnread = 101,
      FilterImportant = 102,
      FilterToday = 103,
      FilterYesterday = 104,
      FilterLast24Hours = 105,
      FilterLast48Hours = 106,
      FilterThisWeek = 107,
      FilterLastWeek = 108
    };

    explicit MessagesProxyModel(MessagesModel* source_model, QObject* parent = nullptr);
    virtual ~MessagesProxyModel();

    QModelIndex getNextPreviousImportantItemIndex(int default_row);
    QModelIndex getNextPreviousUnreadItemIndex(int default_row);

    // Maps list of indexes.
    QModelIndexList mapListToSource(const QModelIndexList& indexes) const;
    QModelIndexList mapListFromSource(const QModelIndexList& indexes, bool deep = false) const;

    // Fix for matching indexes with respect to specifics of the message model.
    QModelIndexList match(const QModelIndex& start, int role, const QVariant& entered_value, int hits, Qt::MatchFlags flags) const;

    // Performs sort of items.
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);

    bool showUnreadOnly() const;
    void setShowUnreadOnly(bool show_unread_only);
    void setFilter(MessageFilter filter);

  private:
    QModelIndex getNextImportantItemIndex(int default_row, int max_row) const;
    QModelIndex getNextUnreadItemIndex(int default_row, int max_row) const;

    bool lessThan(const QModelIndex& left, const QModelIndex& right) const;
    bool acceptMessage(Message currentMessage) const;
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const;

    // Source model pointer.
    MessagesModel* m_sourceModel;
    bool m_showUnreadOnly;
    MessageFilter m_filter;
};

Q_DECLARE_METATYPE(MessagesProxyModel::MessageFilter)

#endif // MESSAGESPROXYMODEL_H
