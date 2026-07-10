#ifndef KEYMAPEDITORDIALOG_H
#define KEYMAPEDITORDIALOG_H

#include <QDialog>
#include <QGraphicsEllipseItem>
#include <QJsonDocument>
#include <QMap>
#include <QPixmap>

class QCheckBox;
class QDoubleSpinBox;
class QGraphicsScene;
class QGraphicsTextItem;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

class KeyMapEditorDialog;

class KeyMapPointItem : public QGraphicsEllipseItem
{
public:
    KeyMapPointItem(KeyMapEditorDialog *editor, const QString &path, const QString &label, QGraphicsItem *parent = 0);

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    QString path() const;
    void setLabelText(const QString &label);

private:
    KeyMapEditorDialog *m_editor = 0;
    QString m_path;
    QGraphicsTextItem *m_label = 0;
};

class KeyMapEditorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit KeyMapEditorDialog(const QString &scriptPath, QWidget *parent = 0);

    void pointMoved(const QString &path, const QPointF &scenePos);

private slots:
    void loadBackground();
    void saveScript();
    void saveScriptAs();
    void selectFromList(QListWidgetItem *item);
    void updateSelectedPoint();
    void updateSelectedFields();
    void toggleGrid(bool checked);
    void toggleLabels(bool checked);
    void selectFromScene();
    void addClickNode();
    void addDragNode();
    void deleteSelectedNode();
    void updateSwitchKey();

private:
    struct PointRef
    {
        QString path;
        QString title;
        QString type;
        QString key;
        QString comment;
        double x = 0;
        double y = 0;
    };

    bool loadScript();
    bool writeScript(const QString &path);
    void buildUi();
    void rebuildScene();
    void rebuildPointList();
    void addGrid();
    void addPoint(const PointRef &point);
    QList<PointRef> collectPoints() const;
    PointRef pointForPath(const QString &path) const;
    void selectPath(const QString &path);
    void setPointInDocument(const QString &path, double x, double y);
    void setFieldInDocument(const QString &path, const QString &field, const QString &value);
    void setBoolFieldInDocument(const QString &path, const QString &field, bool value);
    void updateItemPosition(const QString &path, double x, double y);
    QSizeF logicalSize() const;
    QPointF toScene(double x, double y) const;
    QPointF toRelative(const QPointF &scenePos) const;
    QString nodePrefixForPath(const QString &path) const;
    bool boolFieldForPath(const QString &path, const QString &field) const;
    void setStatus(const QString &text);

    QString m_scriptPath;
    QJsonDocument m_doc;
    bool m_updating = false;
    bool m_showGrid = true;
    bool m_showLabels = true;
    QSizeF m_canvasSize = QSizeF(1280, 720);
    QPixmap m_background;

    QGraphicsView *m_view = 0;
    QGraphicsScene *m_scene = 0;
    QListWidget *m_list = 0;
    QLineEdit *m_typeEdit = 0;
    QLineEdit *m_switchKeyEdit = 0;
    QLineEdit *m_keyEdit = 0;
    QLineEdit *m_commentEdit = 0;
    QCheckBox *m_switchMapCheck = 0;
    QDoubleSpinBox *m_xSpin = 0;
    QDoubleSpinBox *m_ySpin = 0;
    QLabel *m_status = 0;
    QCheckBox *m_gridCheck = 0;
    QCheckBox *m_labelCheck = 0;
    QMap<QString, KeyMapPointItem *> m_items;
};

#endif // KEYMAPEDITORDIALOG_H
