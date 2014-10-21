#include "treeview.h"

#include <QtGui/QGuiApplication>

#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>

#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionFocusRect>

#include "treemodel.h"

#include <QDebug>

TreeView::TreeView(QWidget *parent) :
    QAbstractScrollArea (parent)
{
	setMouseTracking(true);
}

TreeModel *TreeView::model() const
{
	return _model;
}

void TreeView::setModel(TreeModel *model)
{
	_model = model;

	_highlightedItem = 0;
	_selectedItems.clear();

	_offsetY = 0;
	_topNode.obj = model->root();
	_topNode.row = 0;
	_topNode.level = 0;
	_topNode.size = cellSizeHint(0,-1,_topNode.obj);

	calculateTotalHeight();

	verticalScrollBar()->setMaximum( _totalHeight );
}

QSize TreeView::cellSizeHint(int row, int col, QObject *obj) const
{
	return QSize(128, 64);
}

void TreeView::drawCell(int row, int col, QObject *obj, const QRect &cell, QStyleOptionViewItem &opt, QPainter &painter)
{
	QStyleOptionViewItem o;
	o.QStyleOption::operator=(opt);

	o.rect = cell;
	o.displayAlignment = Qt::AlignCenter;

	o.features |= QStyleOptionViewItem::HasDisplay;

	if ( model()->flags(obj) & TreeModel::ItemIsEdited )
		o.text = QString("edited");
	else
		o.text = QString("(%1,%2)").arg(row).arg(col);

	style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &o, &painter, this);
	style()->drawControl(QStyle::CE_ItemViewItem, &o, &painter, this);
}

void TreeView::drawRow(int row, QObject *obj, const QRect &rect, QStyleOptionViewItem &opt, QPainter &painter)
{
	QRect cell(rect);

	//drawGap();
	if ( _highlightedItem == obj )
		opt.state |= QStyle::State_MouseOver;

	if ( row % 2 )
		opt.features |= QStyleOptionViewItem::Alternate;
	else
		opt.features &= ~QStyleOptionViewItem::Alternate;

	style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, &painter, this);

	for(uint i = 0; i < model()->columnCount(obj); i++)
	{
		drawCell(row, i, obj, cell, opt, painter);
		cell.moveLeft( cell.x() + cell.width() );
	}

	if ( _selectedItems.contains(obj)  )
	{
		QStyleOptionFocusRect o;
		o.QStyleOption::operator=(opt);
		o.state |= QStyle::State_KeyboardFocusChange;
		QPalette::ColorGroup cg = QPalette::Normal;
		o.backgroundColor = QGuiApplication::palette().color(cg, QPalette::Highlight);
		o.rect = opt.rect;
		style()->drawPrimitive(QStyle::PE_FrameFocusRect, &o, &painter, this);
	}
}

void TreeView::drawTree(QPainter &painter)
{
	QRect rect;
	QStyleOptionViewItem opt;

	opt.init(this);
	opt.state &= ~QStyle::State_Selected;
	opt.state &= ~QStyle::State_HasFocus;

	rect.setY(_offsetY);

	foreach(const NodeInfo &node, _paintList)
	{
		rect.setX( node.level * 32 );
		rect.setSize(node.size);

		opt.rect.setRect(0, rect.y(), viewport()->width(), rect.height());

		drawRow(node.row, node.obj, rect, opt, painter);

		rect.moveTop( rect.y() + rect.height() );
	}
}

void TreeView::keyPressEvent(QKeyEvent *event)
{
	QObject *t;
	int l;
	TreeModel::Flags flags;

	switch ( event->key() )
	{
		case Qt::Key_Up:
			if ( event->modifiers() != Qt::NoModifier )
				break;

			if ( _selectedItems.empty() )
				break;

			t = _selectedItems.last();
			t = previousNode(t, l);
			if ( !t )
				break;
			_selectedItems.clear();
			_selectedItems.append(t);
			event->accept();
			viewport()->update();
			return;

		case Qt::Key_Down:
			if ( event->modifiers() != Qt::NoModifier )
				break;

			if ( _selectedItems.empty() )
				break;

			t = _selectedItems.last();
			t = nextNode(t, l);
			if ( !t )
				break;
			_selectedItems.clear();
			_selectedItems.append(t);
			event->accept();
			viewport()->update();
			return;

		case Qt::Key_F2:
			if ( event->modifiers() != Qt::NoModifier )
				break;

			if ( _selectedItems.empty() )
				break;

			t = _selectedItems.last();

			if ( !t )
				break;

			flags = model()->flags(t);

			if ( !(flags & TreeModel::ItemIsEditable) )
				break;

			model()->setFlags( flags | TreeModel::ItemIsEdited, t);

			event->accept();
			viewport()->update();
			return;

		case Qt::Key_Escape:
			if ( event->modifiers() != Qt::NoModifier )
				break;

			if ( _selectedItems.empty() )
				break;

			t = _selectedItems.last();

			if ( !t )
				break;

			flags = model()->flags(t);

			if ( flags & TreeModel::ItemIsEdited )
				flags &= ~TreeModel::ItemIsEdited;
			else if ( flags & TreeModel::ItemIsSelected )
				flags &= ~TreeModel::ItemIsSelected;

			model()->setFlags( flags, t);

			//_selectedItems.clear();

			event->accept();
			viewport()->update();
			return;
	}

	QAbstractScrollArea::keyPressEvent(event);
}

void TreeView::mouseMoveEvent(QMouseEvent *event)
{
	QAbstractScrollArea::mouseMoveEvent(event);

	if ( !hasMouseTracking() )
		return;

	_highlightedItem = objAtPos(event->pos());
	viewport()->update();
	event->accept();
}

void TreeView::mousePressEvent(QMouseEvent *event)
{
	QAbstractScrollArea::mousePressEvent(event);

	if ( event->button() != Qt::LeftButton )
		return;

	selectRow(event->pos(), event->modifiers() & Qt::ControlModifier );
	event->accept();
}

void TreeView::paintEvent(QPaintEvent *event)
{
	QAbstractScrollArea::paintEvent(event);

	QPainter painter(viewport());
	drawTree(painter);
	event->accept();
}

void TreeView::resizeEvent(QResizeEvent *event)
{
	QAbstractScrollArea::resizeEvent(event);

	makePaintList(event->size());
	event->accept();
}

void TreeView::scrollContentsBy(int dx, int dy)
{
	_offsetY = findTopNode(_offsetY + dy);
	makePaintList(viewport()->size());
	viewport()->update();
}

int TreeView::findTopNode(int offset)
{
	QObject *t;
	int level;

	level = _topNode.level;

	if ( offset > 0 )
	{
		// scrolling up
		while ( offset > 0 )
		{
			t = previousNode(_topNode.obj, level);

			if ( !t )
				return 0;

			_topNode.obj = t;
			--_topNode.row;
			_topNode.level = level;
			_topNode.size = cellSizeHint(_topNode.row, -1, t);

			offset -= _topNode.size.height();
		}
	}
	else
	{
		// scrolling down
		while ( abs(offset) > _topNode.size.height() )
		{
			offset += _topNode.size.height();

			t = nextNode(_topNode.obj, level);

			if ( !t )
				return 0;//offset;

			_topNode.obj = t;
			++_topNode.row;
			_topNode.level = level;
			_topNode.size = cellSizeHint(_topNode.row, -1, t);
		}
	}

	return offset;
}

void TreeView::makePaintList(const QSize &viewport)
{
	QObject *obj;
	int remainHeight;
	int row;
	int level;

	remainHeight = viewport.height() - _offsetY;

	row = _topNode.row;
	level = _topNode.level;
	obj = _topNode.obj;
	_paintList.clear();

	while ( obj && remainHeight > 0 )
	{
		_paintList.append(NodeInfo());
		_paintList.last().obj = obj;
		_paintList.last().row = row;
		_paintList.last().level = level;
		_paintList.last().size = cellSizeHint(row, -1, obj);

		row++;
		remainHeight -= _paintList.last().size.height();

		obj = nextNode(obj, level);
	}
}

void TreeView::calculateTotalHeight()
{
	QObject *t;
	int level;

	t = model()->root();
	_totalHeight = 0;

	while ( (t = nextNode(t, level)) )
		_totalHeight += cellSizeHint(-1, -1, t).height();
}

void TreeView::selectRow(const QPoint &pos, const bool append)
{
	QObject *obj = objAtPos(pos);

	if ( !obj )
		return;

	TreeModel::Flags flags = model()->flags(obj);

	if ( flags & TreeModel::ItemIsSelectable )
	{
		if ( !append )
			_selectedItems.clear();

		_selectedItems.append(obj);
		viewport()->update();
	}
}

QObject *TreeView::objAtPos(const QPoint &pos) const
{
	int height = pos.y() - _offsetY;

	foreach(const NodeInfo &node, _paintList)
	{
		if ( height < node.size.height() )
			return node.obj;

		height -= node.size.height();
	}

	return 0;
}

QObject *TreeView::previousNode(QObject *obj, int &level) const
{
	QObject *t;

	t = model()->previousSibling(obj);
	if ( t )
	{
		obj = t;
		t = model()->lastChild(t);
		if ( t )
		{
			++level;
			return t;
		}
		else
			return obj;
	}
	else
	{
		--level;
		return model()->parent(obj);
	}
}

QObject *TreeView::nextNode(QObject *obj, int &level) const
{
	QObject *t;

	t = model()->firstChild(obj);
	if ( t )
	{
		++level;
		return t;
	}

	t = model()->nextSibling(obj);
	if ( t )
		return t;

	--level;
	return model()->nextSibling(model()->parent(obj));
}
