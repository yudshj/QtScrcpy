#include "keymapeditordialog.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsLineItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QSplitter>
#include <QVBoxLayout>

KeyMapPointItem::KeyMapPointItem(KeyMapEditorDialog *editor, const QString &path, const QString &label, QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent), m_editor(editor), m_path(path)
{
    setRect(-12, -12, 24, 24);
    setBrush(QColor(244, 197, 66, 220));
    setPen(QPen(QColor(24, 34, 48), 2));
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);
    setZValue(10);

    m_label = new QGraphicsTextItem(label, this);
    m_label->setDefaultTextColor(Qt::white);
    m_label->setPos(14, -26);
    m_label->setZValue(11);
}

QVariant KeyMapPointItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemPositionHasChanged && m_editor) {
        m_editor->pointMoved(m_path, value.toPointF());
    }
    return QGraphicsEllipseItem::itemChange(change, value);
}

QString KeyMapPointItem::path() const
{
    return m_path;
}

void KeyMapPointItem::setLabelText(const QString &label)
{
    if (m_label) {
        m_label->setPlainText(label);
    }
}

KeyMapEditorDialog::KeyMapEditorDialog(const QString &scriptPath, const std::function<QImage()> &captureFrame, QWidget *parent)
    : QDialog(parent), m_scriptPath(scriptPath), m_captureFrame(captureFrame)
{
    buildUi();
    if (loadScript()) {
        rebuildScene();
        rebuildPointList();
        setStatus(tr("Loaded %1").arg(scriptPath));
    }
    if (m_captureFrame) {
        setBackground(m_captureFrame());
    }
}

QString KeyMapEditorDialog::scriptPath() const
{
    return m_scriptPath;
}

bool KeyMapEditorDialog::wasSaved() const
{
    return m_wasSaved;
}

void KeyMapEditorDialog::buildUi()
{
    setWindowTitle(tr("Keymap Editor"));
    resize(1180, 760);

    m_scene = new QGraphicsScene(this);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &KeyMapEditorDialog::selectFromScene);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setDragMode(QGraphicsView::RubberBandDrag);
    m_view->setMinimumSize(620, 420);

    m_list = new QListWidget(this);
    m_list->setMinimumWidth(260);
    connect(m_list, &QListWidget::itemClicked, this, &KeyMapEditorDialog::selectFromList);

    m_typeEdit = new QLineEdit(this);
    m_typeEdit->setReadOnly(true);
    m_switchKeyEdit = new QLineEdit(this);
    connect(m_switchKeyEdit, &QLineEdit::editingFinished, this, &KeyMapEditorDialog::updateSwitchKey);
    m_keyEdit = new QLineEdit(this);
    m_commentEdit = new QLineEdit(this);
    connect(m_keyEdit, &QLineEdit::editingFinished, this, &KeyMapEditorDialog::updateSelectedFields);
    connect(m_commentEdit, &QLineEdit::editingFinished, this, &KeyMapEditorDialog::updateSelectedFields);
    m_switchMapCheck = new QCheckBox(this);
    connect(m_switchMapCheck, &QCheckBox::toggled, this, &KeyMapEditorDialog::updateSelectedFields);

    m_xSpin = new QDoubleSpinBox(this);
    m_ySpin = new QDoubleSpinBox(this);
    foreach (QDoubleSpinBox *spin, QList<QDoubleSpinBox *>() << m_xSpin << m_ySpin) {
        spin->setRange(0.0, 1.0);
        spin->setDecimals(4);
        spin->setSingleStep(0.001);
        connect(spin, SIGNAL(valueChanged(double)), this, SLOT(updateSelectedPoint()));
    }

    QFormLayout *form = new QFormLayout;
    form->addRow(tr("SwitchKey"), m_switchKeyEdit);
    form->addRow(tr("Type"), m_typeEdit);
    form->addRow(tr("Key"), m_keyEdit);
    form->addRow(tr("Comment"), m_commentEdit);
    form->addRow(tr("switchMap"), m_switchMapCheck);
    form->addRow(tr("X"), m_xSpin);
    form->addRow(tr("Y"), m_ySpin);

    QPushButton *backgroundBtn = new QPushButton(tr("Load background"), this);
    QPushButton *captureBtn = new QPushButton(tr("Capture current frame"), this);
    QPushButton *addClickBtn = new QPushButton(tr("Add click"), this);
    QPushButton *addDragBtn = new QPushButton(tr("Add drag"), this);
    QPushButton *deleteBtn = new QPushButton(tr("Delete"), this);
    QPushButton *saveBtn = new QPushButton(tr("Save"), this);
    QPushButton *saveAsBtn = new QPushButton(tr("Save as"), this);
    connect(backgroundBtn, &QPushButton::clicked, this, &KeyMapEditorDialog::loadBackground);
    connect(captureBtn, &QPushButton::clicked, this, &KeyMapEditorDialog::captureBackground);
    connect(addClickBtn, &QPushButton::clicked, this, &KeyMapEditorDialog::addClickNode);
    connect(addDragBtn, &QPushButton::clicked, this, &KeyMapEditorDialog::addDragNode);
    connect(deleteBtn, &QPushButton::clicked, this, &KeyMapEditorDialog::deleteSelectedNode);
    connect(saveBtn, &QPushButton::clicked, this, &KeyMapEditorDialog::saveScript);
    connect(saveAsBtn, &QPushButton::clicked, this, &KeyMapEditorDialog::saveScriptAs);

    m_gridCheck = new QCheckBox(tr("Grid"), this);
    m_gridCheck->setChecked(true);
    m_labelCheck = new QCheckBox(tr("Labels"), this);
    m_labelCheck->setChecked(true);
    connect(m_gridCheck, &QCheckBox::toggled, this, &KeyMapEditorDialog::toggleGrid);
    connect(m_labelCheck, &QCheckBox::toggled, this, &KeyMapEditorDialog::toggleLabels);

    QHBoxLayout *toolLayout = new QHBoxLayout;
    toolLayout->addWidget(backgroundBtn);
    toolLayout->addWidget(captureBtn);
    toolLayout->addWidget(m_gridCheck);
    toolLayout->addWidget(m_labelCheck);
    toolLayout->addWidget(addClickBtn);
    toolLayout->addWidget(addDragBtn);
    toolLayout->addWidget(deleteBtn);
    toolLayout->addStretch();
    toolLayout->addWidget(saveAsBtn);
    toolLayout->addWidget(saveBtn);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(new QLabel(tr("Mapped points"), this));
    leftLayout->addWidget(m_list);

    QWidget *leftWidget = new QWidget(this);
    leftWidget->setLayout(leftLayout);

    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addLayout(form);
    rightLayout->addStretch();
    QWidget *rightWidget = new QWidget(this);
    rightWidget->setLayout(rightLayout);
    rightWidget->setMinimumWidth(260);

    QSplitter *splitter = new QSplitter(this);
    splitter->addWidget(leftWidget);
    splitter->addWidget(m_view);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(1, 1);

    m_status = new QLabel(this);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(toolLayout);
    mainLayout->addWidget(splitter);
    mainLayout->addWidget(m_status);
    setLayout(mainLayout);
}

bool KeyMapEditorDialog::loadScript()
{
    QFile file(m_scriptPath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Keymap Editor"), tr("Open file failed:\n%1").arg(m_scriptPath));
        return false;
    }

    QJsonParseError error;
    m_doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !m_doc.isObject()) {
        QMessageBox::warning(this, tr("Keymap Editor"), tr("JSON parse failed:\n%1").arg(error.errorString()));
        return false;
    }
    if (m_switchKeyEdit) {
        m_switchKeyEdit->setText(m_doc.object().value("switchKey").toString());
    }
    setDirty(false);
    return true;
}

bool KeyMapEditorDialog::writeScript(const QString &path)
{
    updateSwitchKey();
    updateSelectedFields();

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Keymap Editor"), tr("Write file failed:\n%1").arg(path));
        return false;
    }
    const QByteArray data = m_doc.toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        QMessageBox::warning(this, tr("Keymap Editor"), tr("Write file failed:\n%1").arg(path));
        return false;
    }
    m_scriptPath = path;
    m_wasSaved = true;
    setDirty(false);
    setStatus(tr("Saved %1").arg(path));
    return true;
}

void KeyMapEditorDialog::saveScript()
{
    writeScript(m_scriptPath);
}

void KeyMapEditorDialog::saveScriptAs()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save keymap"), m_scriptPath, tr("JSON (*.json)"));
    if (!path.isEmpty()) {
        if (!path.endsWith(".json", Qt::CaseInsensitive)) {
            path += ".json";
        }
        writeScript(path);
    }
}

void KeyMapEditorDialog::loadBackground()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Load background"), QString(), tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty()) {
        return;
    }

    QPixmap pixmap(path);
    if (pixmap.isNull()) {
        QMessageBox::warning(this, tr("Keymap Editor"), tr("Image load failed:\n%1").arg(path));
        return;
    }
    m_background = pixmap;
    m_canvasSize = m_background.size();
    rebuildScene();
    setStatus(tr("Background loaded %1").arg(path));
}

void KeyMapEditorDialog::captureBackground()
{
    if (!m_captureFrame) {
        QMessageBox::warning(this, tr("Keymap Editor"), tr("No connected video source."));
        return;
    }
    const QImage image = m_captureFrame();
    if (image.isNull()) {
        QMessageBox::warning(this, tr("Keymap Editor"), tr("No decoded frame is available yet."));
        return;
    }
    setBackground(image);
    setStatus(tr("Captured current mirrored frame"));
}

void KeyMapEditorDialog::setBackground(const QImage &image)
{
    if (image.isNull()) {
        return;
    }
    m_background = QPixmap::fromImage(image);
    m_canvasSize = image.size();
    rebuildScene();
}

void KeyMapEditorDialog::rebuildScene()
{
    m_updating = true;
    m_scene->clear();
    m_items.clear();
    m_scene->setSceneRect(QRectF(QPointF(0, 0), logicalSize()));
    if (!m_background.isNull()) {
        QGraphicsPixmapItem *bg = m_scene->addPixmap(m_background);
        bg->setZValue(-20);
    }
    if (m_showGrid) {
        addGrid();
    }
    QList<PointRef> points = collectPoints();
    for (int i = 0; i < points.size(); ++i) {
        addPoint(points.at(i));
    }
    m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    m_updating = false;
}

void KeyMapEditorDialog::addGrid()
{
    QSizeF size = logicalSize();
    QPen minor(QColor(255, 80, 80, 110));
    QPen origin(QColor(80, 190, 110, 170));
    for (int i = 0; i <= 10; ++i) {
        double x = size.width() * i / 10.0;
        double y = size.height() * i / 10.0;
        m_scene->addLine(x, 0, x, size.height(), i == 0 ? origin : minor)->setZValue(-10);
        m_scene->addLine(0, y, size.width(), y, i == 0 ? origin : minor)->setZValue(-10);
    }
}

void KeyMapEditorDialog::addPoint(const PointRef &point)
{
    QString label = point.key.isEmpty() ? point.title : point.key;
    KeyMapPointItem *item = new KeyMapPointItem(this, point.path, label);
    item->setToolTip(point.title);
    item->setPos(toScene(point.x, point.y));
    item->setVisible(true);
    QList<QGraphicsItem *> children = item->childItems();
    for (int i = 0; i < children.size(); ++i) {
        children.at(i)->setVisible(m_showLabels);
    }
    m_scene->addItem(item);
    m_items.insert(point.path, item);
}

void KeyMapEditorDialog::rebuildPointList()
{
    m_list->clear();
    QList<PointRef> points = collectPoints();
    for (int i = 0; i < points.size(); ++i) {
        const PointRef point = points.at(i);
        QListWidgetItem *item = new QListWidgetItem(QString("%1  %2").arg(point.key, point.title), m_list);
        item->setData(Qt::UserRole, point.path);
        item->setToolTip(point.path);
    }
}

QList<KeyMapEditorDialog::PointRef> KeyMapEditorDialog::collectPoints() const
{
    QList<PointRef> points;
    QJsonObject root = m_doc.object();

    QJsonObject mouse = root.value("mouseMoveMap").toObject();
    if (mouse.contains("startPos")) {
        QJsonObject pos = mouse.value("startPos").toObject();
        PointRef ref;
        ref.path = "mouseMoveMap.startPos";
        ref.title = tr("mouse view");
        ref.type = "mouseMoveMap";
        ref.key = "mouse";
        ref.x = pos.value("x").toDouble();
        ref.y = pos.value("y").toDouble();
        points << ref;
    }
    QJsonObject smallEyes = mouse.value("smallEyes").toObject();
    if (smallEyes.contains("pos")) {
        QJsonObject pos = smallEyes.value("pos").toObject();
        PointRef ref;
        ref.path = "mouseMoveMap.smallEyes.pos";
        ref.title = smallEyes.value("comment").toString(tr("small eyes"));
        ref.type = smallEyes.value("type").toString("KMT_CLICK");
        ref.key = smallEyes.value("key").toString();
        ref.comment = smallEyes.value("comment").toString();
        ref.x = pos.value("x").toDouble();
        ref.y = pos.value("y").toDouble();
        points << ref;
    }

    QJsonArray nodes = root.value("keyMapNodes").toArray();
    for (int i = 0; i < nodes.size(); ++i) {
        QJsonObject node = nodes.at(i).toObject();
        QString type = node.value("type").toString();
        QString comment = node.value("comment").toString(type);
        QString key = node.value("key").toString();

        if (type == "KMT_CLICK" || type == "KMT_CLICK_TWICE") {
            QJsonObject pos = node.value("pos").toObject();
            PointRef ref;
            ref.path = QString("keyMapNodes.%1.pos").arg(i);
            ref.title = comment;
            ref.type = type;
            ref.key = key;
            ref.comment = comment;
            ref.x = pos.value("x").toDouble();
            ref.y = pos.value("y").toDouble();
            points << ref;
        } else if (type == "KMT_DRAG") {
            QStringList names;
            names << "startPos" << "endPos";
            for (int n = 0; n < names.size(); ++n) {
                QJsonObject pos = node.value(names.at(n)).toObject();
                PointRef ref;
                ref.path = QString("keyMapNodes.%1.%2").arg(i).arg(names.at(n));
                ref.title = comment + (names.at(n) == "startPos" ? tr(" start") : tr(" end"));
                ref.type = type;
                ref.key = key;
                ref.comment = comment;
                ref.x = pos.value("x").toDouble();
                ref.y = pos.value("y").toDouble();
                points << ref;
            }
        } else if (type == "KMT_STEER_WHEEL") {
            QJsonObject pos = node.value("centerPos").toObject();
            PointRef ref;
            ref.path = QString("keyMapNodes.%1.centerPos").arg(i);
            ref.title = comment;
            ref.type = type;
            ref.key = "WASD";
            ref.comment = comment;
            ref.x = pos.value("x").toDouble();
            ref.y = pos.value("y").toDouble();
            points << ref;
        } else if (type == "KMT_CLICK_MULTI") {
            QJsonArray clicks = node.value("clickNodes").toArray();
            for (int j = 0; j < clicks.size(); ++j) {
                QJsonObject click = clicks.at(j).toObject();
                QJsonObject pos = click.value("pos").toObject();
                PointRef ref;
                ref.path = QString("keyMapNodes.%1.clickNodes.%2.pos").arg(i).arg(j);
                ref.title = QString("%1 %2").arg(comment).arg(j + 1);
                ref.type = type;
                ref.key = key;
                ref.comment = comment;
                ref.x = pos.value("x").toDouble();
                ref.y = pos.value("y").toDouble();
                points << ref;
            }
        }
    }
    return points;
}

KeyMapEditorDialog::PointRef KeyMapEditorDialog::pointForPath(const QString &path) const
{
    QList<PointRef> points = collectPoints();
    for (int i = 0; i < points.size(); ++i) {
        if (points.at(i).path == path) {
            return points.at(i);
        }
    }
    return PointRef();
}

void KeyMapEditorDialog::selectFromList(QListWidgetItem *item)
{
    if (!item) {
        return;
    }
    selectPath(item->data(Qt::UserRole).toString());
}

void KeyMapEditorDialog::selectFromScene()
{
    if (m_updating) {
        return;
    }
    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    KeyMapPointItem *point = dynamic_cast<KeyMapPointItem *>(selected.first());
    if (point) {
        selectPath(point->path());
    }
}

void KeyMapEditorDialog::selectPath(const QString &path)
{
    m_updating = true;
    QMap<QString, KeyMapPointItem *>::iterator it;
    for (it = m_items.begin(); it != m_items.end(); ++it) {
        it.value()->setSelected(it.key() == path);
    }
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        if (item->data(Qt::UserRole).toString() == path) {
            m_list->setCurrentItem(item);
            break;
        }
    }
    PointRef point = pointForPath(path);
    m_typeEdit->setText(point.type);
    m_keyEdit->setText(point.key);
    m_commentEdit->setText(point.comment);
    m_switchMapCheck->setChecked(boolFieldForPath(path, "switchMap"));
    m_keyEdit->setEnabled(path.startsWith("keyMapNodes") && point.type != "KMT_STEER_WHEEL");
    m_commentEdit->setEnabled(path.startsWith("keyMapNodes") || path.contains("smallEyes"));
    m_switchMapCheck->setEnabled(point.type == "KMT_CLICK" && (path.startsWith("keyMapNodes") || path.contains("smallEyes")));
    m_xSpin->setValue(point.x);
    m_ySpin->setValue(point.y);
    m_updating = false;
}

void KeyMapEditorDialog::pointMoved(const QString &path, const QPointF &scenePos)
{
    if (m_updating) {
        return;
    }
    QPointF rel = toRelative(scenePos);
    setPointInDocument(path, rel.x(), rel.y());
    PointRef point = pointForPath(path);
    if (point.path == path) {
        m_updating = true;
        m_xSpin->setValue(point.x);
        m_ySpin->setValue(point.y);
        m_updating = false;
    }
}

void KeyMapEditorDialog::updateSelectedPoint()
{
    if (m_updating || !m_list->currentItem()) {
        return;
    }
    QString path = m_list->currentItem()->data(Qt::UserRole).toString();
    setPointInDocument(path, m_xSpin->value(), m_ySpin->value());
    updateItemPosition(path, m_xSpin->value(), m_ySpin->value());
}

void KeyMapEditorDialog::updateSelectedFields()
{
    if (m_updating || !m_list->currentItem()) {
        return;
    }
    QString path = m_list->currentItem()->data(Qt::UserRole).toString();
    setFieldInDocument(path, "key", m_keyEdit->text());
    setFieldInDocument(path, "comment", m_commentEdit->text());
    if (m_switchMapCheck->isEnabled()) {
        setBoolFieldInDocument(path, "switchMap", m_switchMapCheck->isChecked());
    }
    rebuildPointList();
    rebuildScene();
    selectPath(path);
}

void KeyMapEditorDialog::addClickNode()
{
    QJsonObject root = m_doc.object();
    QJsonArray nodes = root.value("keyMapNodes").toArray();

    QJsonObject pos;
    pos.insert("x", 0.5);
    pos.insert("y", 0.5);

    QJsonObject node;
    node.insert("comment", tr("new click"));
    node.insert("type", "KMT_CLICK");
    node.insert("key", "Key_F");
    node.insert("pos", pos);
    node.insert("switchMap", false);

    nodes.append(node);
    root.insert("keyMapNodes", nodes);
    m_doc.setObject(root);
    setDirty();

    QString path = QString("keyMapNodes.%1.pos").arg(nodes.size() - 1);
    rebuildPointList();
    rebuildScene();
    selectPath(path);
}

void KeyMapEditorDialog::addDragNode()
{
    QJsonObject root = m_doc.object();
    QJsonArray nodes = root.value("keyMapNodes").toArray();

    QJsonObject start;
    start.insert("x", 0.5);
    start.insert("y", 0.7);
    QJsonObject end;
    end.insert("x", 0.5);
    end.insert("y", 0.3);

    QJsonObject node;
    node.insert("comment", tr("new drag"));
    node.insert("type", "KMT_DRAG");
    node.insert("key", "Key_Up");
    node.insert("startPos", start);
    node.insert("endPos", end);

    nodes.append(node);
    root.insert("keyMapNodes", nodes);
    m_doc.setObject(root);
    setDirty();

    QString path = QString("keyMapNodes.%1.startPos").arg(nodes.size() - 1);
    rebuildPointList();
    rebuildScene();
    selectPath(path);
}

void KeyMapEditorDialog::deleteSelectedNode()
{
    if (!m_list->currentItem()) {
        return;
    }

    QString path = m_list->currentItem()->data(Qt::UserRole).toString();
    QStringList parts = path.split(".");
    QJsonObject root = m_doc.object();

    if (path == "mouseMoveMap.startPos") {
        root.remove("mouseMoveMap");
    } else if (path == "mouseMoveMap.smallEyes.pos") {
        QJsonObject mouse = root.value("mouseMoveMap").toObject();
        mouse.remove("smallEyes");
        root.insert("mouseMoveMap", mouse);
    } else if (parts.size() >= 2 && parts.at(0) == "keyMapNodes") {
        QJsonArray nodes = root.value("keyMapNodes").toArray();
        int nodeIndex = parts.at(1).toInt();
        if (nodeIndex >= 0 && nodeIndex < nodes.size()) {
            nodes.removeAt(nodeIndex);
            root.insert("keyMapNodes", nodes);
        }
    }

    m_doc.setObject(root);
    setDirty();
    rebuildPointList();
    rebuildScene();
}

void KeyMapEditorDialog::updateSwitchKey()
{
    if (m_updating) {
        return;
    }
    QJsonObject root = m_doc.object();
    root.insert("switchKey", m_switchKeyEdit->text());
    m_doc.setObject(root);
    setDirty();
}

void KeyMapEditorDialog::toggleGrid(bool checked)
{
    m_showGrid = checked;
    rebuildScene();
}

void KeyMapEditorDialog::toggleLabels(bool checked)
{
    m_showLabels = checked;
    rebuildScene();
}

void KeyMapEditorDialog::setPointInDocument(const QString &path, double x, double y)
{
    QJsonObject root = m_doc.object();
    QStringList parts = path.split(".");
    QJsonObject pos;
    pos.insert("x", x);
    pos.insert("y", y);

    if (parts.size() == 2 && parts.at(0) == "mouseMoveMap") {
        QJsonObject mouse = root.value("mouseMoveMap").toObject();
        mouse.insert(parts.at(1), pos);
        root.insert("mouseMoveMap", mouse);
    } else if (path == "mouseMoveMap.smallEyes.pos") {
        QJsonObject mouse = root.value("mouseMoveMap").toObject();
        QJsonObject smallEyes = mouse.value("smallEyes").toObject();
        smallEyes.insert("pos", pos);
        mouse.insert("smallEyes", smallEyes);
        root.insert("mouseMoveMap", mouse);
    } else if (parts.size() >= 3 && parts.at(0) == "keyMapNodes") {
        QJsonArray nodes = root.value("keyMapNodes").toArray();
        int nodeIndex = parts.at(1).toInt();
        QJsonObject node = nodes.at(nodeIndex).toObject();
        if (parts.size() == 3) {
            node.insert(parts.at(2), pos);
        } else if (parts.size() == 5 && parts.at(2) == "clickNodes") {
            QJsonArray clicks = node.value("clickNodes").toArray();
            int clickIndex = parts.at(3).toInt();
            QJsonObject click = clicks.at(clickIndex).toObject();
            click.insert("pos", pos);
            clicks.replace(clickIndex, click);
            node.insert("clickNodes", clicks);
        }
        nodes.replace(nodeIndex, node);
        root.insert("keyMapNodes", nodes);
    }
    m_doc.setObject(root);
    setDirty();
}

void KeyMapEditorDialog::setFieldInDocument(const QString &path, const QString &field, const QString &value)
{
    QString prefix = nodePrefixForPath(path);
    QStringList parts = prefix.split(".");
    QJsonObject root = m_doc.object();

    if (prefix == "mouseMoveMap.smallEyes") {
        QJsonObject mouse = root.value("mouseMoveMap").toObject();
        QJsonObject smallEyes = mouse.value("smallEyes").toObject();
        smallEyes.insert(field, value);
        mouse.insert("smallEyes", smallEyes);
        root.insert("mouseMoveMap", mouse);
    } else if (parts.size() == 2 && parts.at(0) == "keyMapNodes") {
        QJsonArray nodes = root.value("keyMapNodes").toArray();
        int nodeIndex = parts.at(1).toInt();
        QJsonObject node = nodes.at(nodeIndex).toObject();
        if (!(field == "key" && node.value("type").toString() == "KMT_STEER_WHEEL")) {
            node.insert(field, value);
        }
        nodes.replace(nodeIndex, node);
        root.insert("keyMapNodes", nodes);
    }
    m_doc.setObject(root);
    setDirty();
}

void KeyMapEditorDialog::setBoolFieldInDocument(const QString &path, const QString &field, bool value)
{
    QString prefix = nodePrefixForPath(path);
    QStringList parts = prefix.split(".");
    QJsonObject root = m_doc.object();

    if (prefix == "mouseMoveMap.smallEyes") {
        QJsonObject mouse = root.value("mouseMoveMap").toObject();
        QJsonObject smallEyes = mouse.value("smallEyes").toObject();
        smallEyes.insert(field, value);
        mouse.insert("smallEyes", smallEyes);
        root.insert("mouseMoveMap", mouse);
    } else if (parts.size() == 2 && parts.at(0) == "keyMapNodes") {
        QJsonArray nodes = root.value("keyMapNodes").toArray();
        int nodeIndex = parts.at(1).toInt();
        QJsonObject node = nodes.at(nodeIndex).toObject();
        node.insert(field, value);
        nodes.replace(nodeIndex, node);
        root.insert("keyMapNodes", nodes);
    }
    m_doc.setObject(root);
    setDirty();
}

void KeyMapEditorDialog::updateItemPosition(const QString &path, double x, double y)
{
    if (!m_items.contains(path)) {
        return;
    }
    m_updating = true;
    m_items.value(path)->setPos(toScene(x, y));
    m_updating = false;
}

QSizeF KeyMapEditorDialog::logicalSize() const
{
    return m_canvasSize;
}

QPointF KeyMapEditorDialog::toScene(double x, double y) const
{
    QSizeF size = logicalSize();
    return QPointF(x * size.width(), y * size.height());
}

QPointF KeyMapEditorDialog::toRelative(const QPointF &scenePos) const
{
    QSizeF size = logicalSize();
    double x = qMax(0.0, qMin(1.0, scenePos.x() / size.width()));
    double y = qMax(0.0, qMin(1.0, scenePos.y() / size.height()));
    return QPointF(x, y);
}

QString KeyMapEditorDialog::nodePrefixForPath(const QString &path) const
{
    if (path == "mouseMoveMap.smallEyes.pos") {
        return "mouseMoveMap.smallEyes";
    }
    QStringList parts = path.split(".");
    if (parts.size() >= 2 && parts.at(0) == "keyMapNodes") {
        return QString("keyMapNodes.%1").arg(parts.at(1));
    }
    return path;
}

bool KeyMapEditorDialog::boolFieldForPath(const QString &path, const QString &field) const
{
    QString prefix = nodePrefixForPath(path);
    QStringList parts = prefix.split(".");
    QJsonObject root = m_doc.object();

    if (prefix == "mouseMoveMap.smallEyes") {
        QJsonObject mouse = root.value("mouseMoveMap").toObject();
        return mouse.value("smallEyes").toObject().value(field).toBool();
    }
    if (parts.size() == 2 && parts.at(0) == "keyMapNodes") {
        QJsonArray nodes = root.value("keyMapNodes").toArray();
        int nodeIndex = parts.at(1).toInt();
        if (nodeIndex >= 0 && nodeIndex < nodes.size()) {
            return nodes.at(nodeIndex).toObject().value(field).toBool();
        }
    }
    return false;
}

void KeyMapEditorDialog::setStatus(const QString &text)
{
    m_status->setText(text);
}

void KeyMapEditorDialog::setDirty(bool dirty)
{
    m_dirty = dirty;
    setWindowTitle(tr("Keymap Editor") + (m_dirty ? " *" : ""));
}

void KeyMapEditorDialog::closeEvent(QCloseEvent *event)
{
    if (!m_dirty) {
        event->accept();
        return;
    }

    const QMessageBox::StandardButton choice = QMessageBox::question(
        this,
        tr("Keymap Editor"),
        tr("Save changes before closing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Cancel) {
        event->ignore();
        return;
    }
    if (choice == QMessageBox::Save && !writeScript(m_scriptPath)) {
        event->ignore();
        return;
    }
    event->accept();
}
