// #include <QDesktopWidget>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QShortcut>
#include <QStyle>
#include <QStyleOption>
#include <QTimer>
#include <QWindow>
#include <QtWidgets/QHBoxLayout>

#if defined(Q_OS_WIN32)
#include <Windows.h>
#endif

#include "config.h"
#include "iconhelper.h"
#include "qyuvopenglwidget.h"
#include "toolform.h"
#include "mousetap/mousetap.h"
#include "ui_videoform.h"
#include "videoform.h"

struct KeyMapOverlayItem
{
    QString label;
    QString type;
    QPointF start;
    QPointF end;
    bool hasEnd = false;
};

static double bound01(double value)
{
    return qMax(0.0, qMin(1.0, value));
}

static QString displayKeyName(const QString &key)
{
    if (key == "LeftButton") {
        return "LMB";
    }
    if (key == "RightButton") {
        return "RMB";
    }
    if (key.startsWith("Key_")) {
        return key.mid(4);
    }
    return key;
}

class KeyMapOverlayWidget : public QWidget
{
public:
    explicit KeyMapOverlayWidget(QWidget *parent = 0) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
        setAutoFillBackground(false);
    }

    void setItems(const QVector<KeyMapOverlayItem> &items)
    {
        m_items = items;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        for (int i = 0; i < m_items.size(); ++i) {
            drawItem(&painter, m_items.at(i));
        }
    }

private:
    QPointF toWidget(const QPointF &point) const
    {
        return QPointF(point.x() * width(), point.y() * height());
    }

    void drawItem(QPainter *painter, const KeyMapOverlayItem &item)
    {
        QPointF start = toWidget(item.start);
        QColor fill(244, 197, 66, 135);
        QColor stroke(20, 27, 36, 180);
        if (item.type == "KMT_STEER_WHEEL") {
            fill = QColor(47, 128, 237, 125);
        } else if (item.type == "mouseMoveMap") {
            fill = QColor(155, 97, 255, 120);
        } else if (item.hasEnd) {
            fill = QColor(232, 92, 74, 120);
        }

        if (item.hasEnd) {
            QPointF end = toWidget(item.end);
            painter->setPen(QPen(QColor(255, 255, 255, 140), 3));
            painter->drawLine(start, end);
            painter->setBrush(QColor(232, 92, 74, 135));
            painter->setPen(QPen(stroke, 2));
            painter->drawEllipse(end, 10, 10);
        }

        painter->setBrush(fill);
        painter->setPen(QPen(stroke, 2));
        painter->drawEllipse(start, 14, 14);

        QString text = item.label.isEmpty() ? item.type : item.label;
        QFont font = painter->font();
        font.setBold(true);
        font.setPointSize(10);
        painter->setFont(font);
        QRect textRect = painter->fontMetrics().boundingRect(text).adjusted(-8, -5, 8, 5);
        QPoint textTopLeft(qRound(start.x() - textRect.width() / 2), qRound(start.y() - 40));
        textTopLeft.setX(qMax(2, qMin(width() - textRect.width() - 2, textTopLeft.x())));
        textTopLeft.setY(qMax(2, qMin(height() - textRect.height() - 2, textTopLeft.y())));
        textRect.moveTopLeft(textTopLeft);

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(18, 24, 33, 135));
        painter->drawRoundedRect(textRect, 5, 5);
        painter->setPen(QColor(255, 255, 255, 230));
        painter->drawText(textRect, Qt::AlignCenter, text);
    }

    QVector<KeyMapOverlayItem> m_items;
};

VideoForm::VideoForm(bool framelessWindow, bool skin, bool showToolbar, QWidget *parent) : QWidget(parent), ui(new Ui::videoForm), m_skin(skin)
{
    ui->setupUi(this);
    initUI();
    installShortcut();
    updateShowSize(size());
    bool vertical = size().height() > size().width();
    this->show_toolbar = showToolbar;
    if (m_skin) {
        updateStyleSheet(vertical);
    }
    if (framelessWindow) {
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    }
}

VideoForm::~VideoForm()
{
    grabCursor(false);
    delete ui;
}

void VideoForm::initUI()
{
    if (m_skin) {
        QPixmap phone;
        if (phone.load(":/res/phone.png")) {
            m_widthHeightRatio = 1.0f * phone.width() / phone.height();
        }

#ifndef Q_OS_OSX
        // mac下去掉标题栏影响showfullscreen
        // 去掉标题栏
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
        // 根据图片构造异形窗口
        setAttribute(Qt::WA_TranslucentBackground);
#endif
    }

    m_videoWidget = new QYUVOpenGLWidget();
    m_videoWidget->setFocusPolicy(Qt::NoFocus);
    m_videoWidget->hide();
    ui->keepRatioWidget->setWidget(m_videoWidget);
    ui->keepRatioWidget->setWidthHeightRatio(m_widthHeightRatio);

    m_keyMapOverlay = new KeyMapOverlayWidget(ui->keepRatioWidget);
    m_keyMapOverlay->hide();

    m_fpsLabel = new QLabel(m_videoWidget);
    QFont ft;
    ft.setPointSize(15);
    ft.setWeight(QFont::Light);
    ft.setBold(true);
    m_fpsLabel->setFont(ft);
    m_fpsLabel->move(5, 15);
    m_fpsLabel->setMinimumWidth(100);
    m_fpsLabel->setStyleSheet(R"(QLabel {color: #00FF00;})");

    m_keyMapModeLabel = new QLabel(m_videoWidget);
    m_keyMapModeLabel->setAlignment(Qt::AlignCenter);
    m_keyMapModeLabel->setStyleSheet(R"(
        QLabel {
            color: white;
            background: rgba(18, 24, 33, 190);
            border: 1px solid rgba(255, 255, 255, 150);
            border-radius: 4px;
            padding: 6px 12px;
            font-size: 16px;
            font-weight: 700;
        }
    )");
    m_keyMapModeLabel->hide();

    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    m_videoWidget->setMouseTracking(true);
    ui->keepRatioWidget->setMouseTracking(true);
}

QRect VideoForm::getGrabCursorRect()
{
    QRect rc;
#if defined(Q_OS_WIN32)
    rc = QRect(ui->keepRatioWidget->mapToGlobal(m_videoWidget->pos()), m_videoWidget->size());
    // high dpi support
    rc.setTopLeft(rc.topLeft() * m_videoWidget->devicePixelRatioF());
    rc.setBottomRight(rc.bottomRight() * m_videoWidget->devicePixelRatioF());

    rc.setX(rc.x() + 10);
    rc.setY(rc.y() + 10);
    rc.setWidth(rc.width() - 20);
    rc.setHeight(rc.height() - 20);
#elif defined(Q_OS_OSX)
    rc = m_videoWidget->geometry();
    rc.setTopLeft(ui->keepRatioWidget->mapToGlobal(rc.topLeft()));
    rc.setBottomRight(ui->keepRatioWidget->mapToGlobal(rc.bottomRight()));

    rc.setX(rc.x() + 10);
    rc.setY(rc.y() + 10);
    rc.setWidth(rc.width() - 20);
    rc.setHeight(rc.height() - 20);
#elif defined(Q_OS_LINUX)
    rc = QRect(ui->keepRatioWidget->mapToGlobal(m_videoWidget->pos()), m_videoWidget->size());
    // high dpi support -- taken from the WIN32 section and untested
    rc.setTopLeft(rc.topLeft() * m_videoWidget->devicePixelRatioF());
    rc.setBottomRight(rc.bottomRight() * m_videoWidget->devicePixelRatioF());

    rc.setX(rc.x() + 10);
    rc.setY(rc.y() + 10);
    rc.setWidth(rc.width() - 20);
    rc.setHeight(rc.height() - 20);
#endif
    return rc;
}

const QSize &VideoForm::frameSize()
{
    return m_frameSize;
}

void VideoForm::resizeSquare()
{
    QRect screenRect = getScreenRect();
    if (screenRect.isEmpty()) {
        qWarning() << "getScreenRect is empty";
        return;
    }
    resize(screenRect.height(), screenRect.height());
}

void VideoForm::removeBlackRect()
{
    resize(ui->keepRatioWidget->goodSize());
}

void VideoForm::showFPS(bool show)
{
    if (!m_fpsLabel) {
        return;
    }
    m_fpsLabel->setVisible(show);
}

void VideoForm::updateRender(int width, int height, uint8_t* dataY, uint8_t* dataU, uint8_t* dataV, int linesizeY, int linesizeU, int linesizeV)
{
    if (m_videoWidget->isHidden()) {
        if (m_loadingWidget) {
            m_loadingWidget->close();
        }
        m_videoWidget->show();
    }

    updateShowSize(QSize(width, height));
    m_videoWidget->setFrameSize(QSize(width, height));
    m_videoWidget->updateTextures(dataY, dataU, dataV, linesizeY, linesizeU, linesizeV);
    updateKeyMapOverlayGeometry();
}

void VideoForm::setSerial(const QString &serial)
{
    m_serial = serial;
}

void VideoForm::setKeyMapScript(const QString &script)
{
    m_keyMapScript = script;
    parseKeyMapOverlayItems(script);
}

void VideoForm::setKeyMapOverlayVisible(bool visible)
{
    m_keyMapOverlayVisible = visible;
    if (m_keyMapOverlay) {
        m_keyMapOverlay->setVisible(visible && !m_keyMapScript.trimmed().isEmpty());
        m_keyMapOverlay->raise();
    }
    if (m_toolForm) {
        m_toolForm->setKeyMapOverlayVisible(visible);
    }
}

void VideoForm::toggleKeyMapOverlay()
{
    setKeyMapOverlayVisible(!m_keyMapOverlayVisible);
}

bool VideoForm::keyMapOverlayVisible() const
{
    return m_keyMapOverlayVisible;
}

void VideoForm::showToolForm(bool show)
{
    if (!m_toolForm) {
        m_toolForm = new ToolForm(this, ToolForm::AP_OUTSIDE_RIGHT);
        m_toolForm->setSerial(m_serial);
        m_toolForm->setKeyMapOverlayVisible(m_keyMapOverlayVisible);
    }
    m_toolForm->move(pos().x() + geometry().width(), pos().y() + 30);
    m_toolForm->setVisible(show);
}

void VideoForm::moveCenter()
{
    QRect screenRect = getScreenRect();
    if (screenRect.isEmpty()) {
        qWarning() << "getScreenRect is empty";
        return;
    }
    // 窗口居中
    move(screenRect.center() - QRect(0, 0, size().width(), size().height()).center());
}

void VideoForm::installShortcut()
{
    QShortcut *shortcut = nullptr;

    // switchFullScreen
    shortcut = new QShortcut(QKeySequence("Ctrl+f"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        switchFullScreen();
    });

    // resizeSquare
    shortcut = new QShortcut(QKeySequence("Ctrl+g"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() { resizeSquare(); });

    // removeBlackRect
    shortcut = new QShortcut(QKeySequence("Ctrl+w"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() { removeBlackRect(); });

    // keymap overlay
    shortcut = new QShortcut(QKeySequence("Ctrl+k"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() { toggleKeyMapOverlay(); });

    // postGoHome
    shortcut = new QShortcut(QKeySequence("Ctrl+h"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        device->postGoHome();
    });

    // postGoBack
    shortcut = new QShortcut(QKeySequence("Ctrl+b"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        device->postGoBack();
    });

    // postAppSwitch
    shortcut = new QShortcut(QKeySequence("Ctrl+s"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->postAppSwitch();
    });

    // postGoMenu
    shortcut = new QShortcut(QKeySequence("Ctrl+m"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        device->postGoMenu();
    });

    // postVolumeUp
    shortcut = new QShortcut(QKeySequence("Ctrl+up"), this);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->postVolumeUp();
    });

    // postVolumeDown
    shortcut = new QShortcut(QKeySequence("Ctrl+down"), this);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->postVolumeDown();
    });

    // postPower
    shortcut = new QShortcut(QKeySequence("Ctrl+p"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->postPower();
    });

    shortcut = new QShortcut(QKeySequence("Ctrl+o"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->setDisplayPower(false);
    });

    // expandNotificationPanel
    shortcut = new QShortcut(QKeySequence("Ctrl+n"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->expandNotificationPanel();
    });

    // collapsePanel
    shortcut = new QShortcut(QKeySequence("Ctrl+Shift+n"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->collapsePanel();
    });

    // copy
    shortcut = new QShortcut(QKeySequence("Ctrl+c"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->postCopy();
    });

    // cut
    shortcut = new QShortcut(QKeySequence("Ctrl+x"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->postCut();
    });

    // clipboardPaste
    shortcut = new QShortcut(QKeySequence("Ctrl+v"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->setDeviceClipboard();
    });

    // setDeviceClipboard
    shortcut = new QShortcut(QKeySequence("Ctrl+Shift+v"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->clipboardPaste();
    });
}

QRect VideoForm::getScreenRect()
{
    QRect screenRect;
    QScreen *screen = QGuiApplication::primaryScreen();
    QWidget *win = window();
    if (win) {
        QWindow *winHandle = win->windowHandle();
        if (winHandle) {
            screen = winHandle->screen();
        }
    }

    if (screen) {
        screenRect = screen->availableGeometry();
    }
    return screenRect;
}

void VideoForm::updateStyleSheet(bool vertical)
{
    if (vertical) {
        setStyleSheet(R"(
                 #videoForm {
                     border-image: url(:/image/videoform/phone-v.png) 150px 65px 85px 65px;
                     border-width: 150px 65px 85px 65px;
                 }
                 )");
    } else {
        setStyleSheet(R"(
                 #videoForm {
                     border-image: url(:/image/videoform/phone-h.png) 65px 85px 65px 150px;
                     border-width: 65px 85px 65px 150px;
                 }
                 )");
    }
    layout()->setContentsMargins(getMargins(vertical));
}

QMargins VideoForm::getMargins(bool vertical)
{
    QMargins margins;
    if (vertical) {
        margins = QMargins(10, 68, 12, 62);
    } else {
        margins = QMargins(68, 12, 62, 10);
    }
    return margins;
}

void VideoForm::updateShowSize(const QSize &newSize)
{
    if (m_frameSize != newSize) {
        m_frameSize = newSize;

        m_widthHeightRatio = 1.0f * newSize.width() / newSize.height();
        ui->keepRatioWidget->setWidthHeightRatio(m_widthHeightRatio);

        bool vertical = m_widthHeightRatio < 1.0f ? true : false;
        QSize showSize = newSize;
        QRect screenRect = getScreenRect();
        if (screenRect.isEmpty()) {
            qWarning() << "getScreenRect is empty";
            return;
        }
        if (vertical) {
            showSize.setHeight(qMin(newSize.height(), screenRect.height() - 200));
            showSize.setWidth(showSize.height() * m_widthHeightRatio);
        } else {
            showSize.setWidth(qMin(newSize.width(), screenRect.width() / 2));
            showSize.setHeight(showSize.width() / m_widthHeightRatio);
        }

        if (isFullScreen() && qsc::IDeviceManage::getInstance().getDevice(m_serial)) {
            switchFullScreen();
        }

        if (isMaximized()) {
            showNormal();
        }

        if (m_skin) {
            QMargins m = getMargins(vertical);
            showSize.setWidth(showSize.width() + m.left() + m.right());
            showSize.setHeight(showSize.height() + m.top() + m.bottom());
        }

        if (showSize != size()) {
            resize(showSize);
            if (m_skin) {
                updateStyleSheet(vertical);
            }
            moveCenter();
        }
    }
}

void VideoForm::switchFullScreen()
{
    if (isFullScreen()) {
        // 横屏全屏铺满全屏，恢复时，恢复保持宽高比
        if (m_widthHeightRatio > 1.0f) {
            ui->keepRatioWidget->setWidthHeightRatio(m_widthHeightRatio);
        }

        showNormal();
        // back to normal size.
        resize(m_normalSize);
        // fullscreen window will move (0,0). qt bug?
        move(m_fullScreenBeforePos);

#ifdef Q_OS_OSX
        //setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
        //show();
#endif
        if (m_skin) {
            updateStyleSheet(m_frameSize.height() > m_frameSize.width());
        }
        showToolForm(this->show_toolbar);
#ifdef Q_OS_WIN32
        ::SetThreadExecutionState(ES_CONTINUOUS);
#endif
    } else {
        // 横屏全屏铺满全屏，不保持宽高比
        if (m_widthHeightRatio > 1.0f) {
            ui->keepRatioWidget->setWidthHeightRatio(-1.0f);
        }

        // record current size before fullscreen, it will be used to rollback size after exit fullscreen.
        m_normalSize = size();

        m_fullScreenBeforePos = pos();
        // 这种临时增加标题栏再全屏的方案会导致收不到mousemove事件，导致setmousetrack失效
        // mac fullscreen must show title bar
#ifdef Q_OS_OSX
        //setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
#endif
        showToolForm(false);
        if (m_skin) {
            layout()->setContentsMargins(0, 0, 0, 0);
        }
        showFullScreen();

        // 全屏状态禁止电脑休眠、息屏
#ifdef Q_OS_WIN32
        ::SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
#endif
    }
}

bool VideoForm::isHost()
{
    if (!m_toolForm) {
        return false;
    }
    return m_toolForm->isHost();
}

void VideoForm::updateFPS(quint32 fps)
{
    //qDebug() << "FPS:" << fps;
    if (!m_fpsLabel) {
        return;
    }
    m_fpsLabel->setText(QString("FPS:%1").arg(fps));
}

void VideoForm::grabCursor(bool grab)
{
    QRect rc = getGrabCursorRect();
    MouseTap::getInstance()->enableMouseEventTap(rc, grab);

    if (m_keyMapModeLabel) {
        m_keyMapModeLabel->setText(grab ? tr("KEYMAP ON") : tr("KEYMAP OFF"));
        m_keyMapModeLabel->adjustSize();
        m_keyMapModeLabel->move(qMax(0, (m_videoWidget->width() - m_keyMapModeLabel->width()) / 2), 20);
        m_keyMapModeLabel->raise();
        m_keyMapModeLabel->show();
        QTimer::singleShot(1200, this, [this]() {
            if (m_keyMapModeLabel) {
                m_keyMapModeLabel->hide();
            }
        });
    }
}

void VideoForm::onFrame(int width, int height, uint8_t *dataY, uint8_t *dataU, uint8_t *dataV, int linesizeY, int linesizeU, int linesizeV)
{
    updateRender(width, height, dataY, dataU, dataV, linesizeY, linesizeU, linesizeV);
}

void VideoForm::staysOnTop(bool top)
{
    bool needShow = false;
    if (isVisible()) {
        needShow = true;
    }
    setWindowFlag(Qt::WindowStaysOnTopHint, top);
    if (m_toolForm) {
        m_toolForm->setWindowFlag(Qt::WindowStaysOnTopHint, top);
    }
    if (needShow) {
        show();
    }
}

void VideoForm::mousePressEvent(QMouseEvent *event)
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (event->button() == Qt::MiddleButton) {
        if (device && !device->isCurrentCustomKeymap()) {
            device->postGoHome();
            return;
        }
    }

    if (event->button() == Qt::RightButton) {
        if (device && !device->isCurrentCustomKeymap()) {
            device->postGoBack();
            return;
        }
    }

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QPointF localPos = event->localPos();
        QPointF globalPos = event->globalPos();
#else
        QPointF localPos = event->position();
        QPointF globalPos = event->globalPosition();
#endif

    if (m_videoWidget->geometry().contains(event->pos())) {
        setFocus(Qt::MouseFocusReason);
        if (!device) {
            return;
        }
        QPointF mappedPos = m_videoWidget->mapFrom(this, localPos.toPoint());
        QMouseEvent newEvent(event->type(), mappedPos, globalPos, event->button(), event->buttons(), event->modifiers());
        emit device->mouseEvent(&newEvent, m_videoWidget->frameSize(), m_videoWidget->size());

        // debug keymap pos
        if (event->button() == Qt::LeftButton) {
            qreal x = localPos.x() / m_videoWidget->size().width();
            qreal y = localPos.y() / m_videoWidget->size().height();
            QString posTip = QString(R"("pos": {"x": %1, "y": %2})").arg(x).arg(y);
            qInfo() << posTip.toStdString().c_str();
        }
    } else {
        if (event->button() == Qt::LeftButton) {
            m_dragPosition = globalPos.toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }
}

void VideoForm::mouseReleaseEvent(QMouseEvent *event)
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (m_dragPosition.isNull()) {
        if (!device) {
            return;
        }
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QPointF localPos = event->localPos();
        QPointF globalPos = event->globalPos();
#else
        QPointF localPos = event->position();
        QPointF globalPos = event->globalPosition();
#endif
        // local check
        QPointF local = m_videoWidget->mapFrom(this, localPos.toPoint());
        if (local.x() < 0) {
            local.setX(0);
        }
        if (local.x() > m_videoWidget->width()) {
            local.setX(m_videoWidget->width());
        }
        if (local.y() < 0) {
            local.setY(0);
        }
        if (local.y() > m_videoWidget->height()) {
            local.setY(m_videoWidget->height());
        }
        QMouseEvent newEvent(event->type(), local, globalPos, event->button(), event->buttons(), event->modifiers());
        emit device->mouseEvent(&newEvent, m_videoWidget->frameSize(), m_videoWidget->size());
    } else {
        m_dragPosition = QPoint(0, 0);
    }
}

void VideoForm::mouseMoveEvent(QMouseEvent *event)
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QPointF localPos = event->localPos();
        QPointF globalPos = event->globalPos();
#else
        QPointF localPos = event->position();
        QPointF globalPos = event->globalPosition();
#endif
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (m_videoWidget->geometry().contains(event->pos())) {
        if (!device) {
            return;
        }
        QPointF mappedPos = m_videoWidget->mapFrom(this, localPos.toPoint());
        QMouseEvent newEvent(event->type(), mappedPos, globalPos, event->button(), event->buttons(), event->modifiers());
        emit device->mouseEvent(&newEvent, m_videoWidget->frameSize(), m_videoWidget->size());
    } else if (!m_dragPosition.isNull()) {
        if (event->buttons() & Qt::LeftButton) {
            move(globalPos.toPoint() - m_dragPosition);
            event->accept();
        }
    }
}

void VideoForm::mouseDoubleClickEvent(QMouseEvent *event)
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (event->button() == Qt::LeftButton && !m_videoWidget->geometry().contains(event->pos())) {
        if (!isMaximized()) {
            removeBlackRect();
        }
    }

    if (event->button() == Qt::RightButton && device && !device->isCurrentCustomKeymap()) {
        emit device->postBackOrScreenOn(event->type() == QEvent::MouseButtonPress);
    }

    if (m_videoWidget->geometry().contains(event->pos())) {
        if (!device) {
            return;
        }
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QPointF localPos = event->localPos();
        QPointF globalPos = event->globalPos();
#else
        QPointF localPos = event->position();
        QPointF globalPos = event->globalPosition();
#endif
        QPointF mappedPos = m_videoWidget->mapFrom(this, localPos.toPoint());
        QMouseEvent newEvent(event->type(), mappedPos, globalPos, event->button(), event->buttons(), event->modifiers());
        emit device->mouseEvent(&newEvent, m_videoWidget->frameSize(), m_videoWidget->size());
    }
}

void VideoForm::wheelEvent(QWheelEvent *event)
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    if (m_videoWidget->geometry().contains(event->position().toPoint())) {
        if (!device) {
            return;
        }
        QPointF pos = m_videoWidget->mapFrom(this, event->position().toPoint());
        QWheelEvent wheelEvent(
            pos, event->globalPosition(), event->pixelDelta(), event->angleDelta(), event->buttons(), event->modifiers(), event->phase(), event->inverted());
#else
    if (m_videoWidget->geometry().contains(event->pos())) {
        if (!device) {
            return;
        }
        QPointF pos = m_videoWidget->mapFrom(this, event->pos());

        QWheelEvent wheelEvent(
            pos, event->globalPosF(), event->pixelDelta(), event->angleDelta(), event->delta(), event->orientation(),
            event->buttons(), event->modifiers(), event->phase(), event->source(), event->inverted());
#endif
        emit device->wheelEvent(&wheelEvent, m_videoWidget->frameSize(), m_videoWidget->size());
    }
}

void VideoForm::keyPressEvent(QKeyEvent *event)
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    if (Qt::Key_Escape == event->key() && !event->isAutoRepeat() && isFullScreen()) {
        switchFullScreen();
    }

    emit device->keyEvent(event, m_videoWidget->frameSize(), m_videoWidget->size());
}

void VideoForm::keyReleaseEvent(QKeyEvent *event)
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    emit device->keyEvent(event, m_videoWidget->frameSize(), m_videoWidget->size());
}

void VideoForm::parseKeyMapOverlayItems(const QString &script)
{
    QVector<KeyMapOverlayItem> items;
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(script.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        if (m_keyMapOverlay) {
            m_keyMapOverlay->setItems(items);
            m_keyMapOverlay->hide();
        }
        return;
    }

    QJsonObject root = doc.object();
    QJsonObject mouse = root.value("mouseMoveMap").toObject();
    QJsonObject mousePos = mouse.value("startPos").toObject();
    if (mousePos.contains("x") && mousePos.contains("y")) {
        KeyMapOverlayItem item;
        item.type = "mouseMoveMap";
        item.label = "Mouse";
        item.start = QPointF(mousePos.value("x").toDouble(), mousePos.value("y").toDouble());
        items << item;
    }

    QJsonObject smallEyes = mouse.value("smallEyes").toObject();
    QJsonObject smallEyesPos = smallEyes.value("pos").toObject();
    if (smallEyesPos.contains("x") && smallEyesPos.contains("y")) {
        KeyMapOverlayItem item;
        item.type = smallEyes.value("type").toString("KMT_CLICK");
        item.label = displayKeyName(smallEyes.value("key").toString("Key_Alt"));
        item.start = QPointF(smallEyesPos.value("x").toDouble(), smallEyesPos.value("y").toDouble());
        items << item;
    }

    QJsonArray nodes = root.value("keyMapNodes").toArray();
    for (int i = 0; i < nodes.size(); ++i) {
        QJsonObject node = nodes.at(i).toObject();
        QString type = node.value("type").toString();
        QString key = node.value("key").toString();

        if (type == "KMT_CLICK" || type == "KMT_CLICK_TWICE") {
            QJsonObject pos = node.value("pos").toObject();
            if (!pos.contains("x") || !pos.contains("y")) {
                continue;
            }
            KeyMapOverlayItem item;
            item.type = type;
            item.label = displayKeyName(key);
            item.start = QPointF(pos.value("x").toDouble(), pos.value("y").toDouble());
            items << item;
        } else if (type == "KMT_DRAG") {
            QJsonObject start = node.value("startPos").toObject();
            QJsonObject end = node.value("endPos").toObject();
            if (!start.contains("x") || !start.contains("y") || !end.contains("x") || !end.contains("y")) {
                continue;
            }
            KeyMapOverlayItem item;
            item.type = type;
            item.label = displayKeyName(key);
            item.start = QPointF(start.value("x").toDouble(), start.value("y").toDouble());
            item.end = QPointF(end.value("x").toDouble(), end.value("y").toDouble());
            item.hasEnd = true;
            items << item;
        } else if (type == "KMT_CLICK_MULTI") {
            QJsonArray clicks = node.value("clickNodes").toArray();
            for (int clickIndex = 0; clickIndex < clicks.size(); ++clickIndex) {
                QJsonObject click = clicks.at(clickIndex).toObject();
                QJsonObject pos = click.value("pos").toObject();
                if (!pos.contains("x") || !pos.contains("y")) {
                    continue;
                }
                KeyMapOverlayItem item;
                item.type = type;
                item.label = QString("%1.%2").arg(displayKeyName(key)).arg(clickIndex + 1);
                item.start = QPointF(pos.value("x").toDouble(), pos.value("y").toDouble());
                items << item;
            }
        } else if (type == "KMT_STEER_WHEEL") {
            QJsonObject center = node.value("centerPos").toObject();
            if (!center.contains("x") || !center.contains("y")) {
                continue;
            }
            double x = center.value("x").toDouble();
            double y = center.value("y").toDouble();
            double leftOffset = node.value("leftOffset").toDouble(0.1);
            double rightOffset = node.value("rightOffset").toDouble(0.1);
            double upOffset = node.value("upOffset").toDouble(0.1);
            double downOffset = node.value("downOffset").toDouble(0.1);

            KeyMapOverlayItem up;
            up.type = type;
            up.label = displayKeyName(node.value("upKey").toString("Key_W"));
            up.start = QPointF(bound01(x), bound01(y - upOffset));
            items << up;

            KeyMapOverlayItem left;
            left.type = type;
            left.label = displayKeyName(node.value("leftKey").toString("Key_A"));
            left.start = QPointF(bound01(x - leftOffset), bound01(y));
            items << left;

            KeyMapOverlayItem down;
            down.type = type;
            down.label = displayKeyName(node.value("downKey").toString("Key_S"));
            down.start = QPointF(bound01(x), bound01(y + downOffset));
            items << down;

            KeyMapOverlayItem right;
            right.type = type;
            right.label = displayKeyName(node.value("rightKey").toString("Key_D"));
            right.start = QPointF(bound01(x + rightOffset), bound01(y));
            items << right;
        }
    }

    if (m_keyMapOverlay) {
        m_keyMapOverlay->setItems(items);
        m_keyMapOverlay->setVisible(m_keyMapOverlayVisible && !items.isEmpty());
        m_keyMapOverlay->raise();
    }
    updateKeyMapOverlayGeometry();
}

void VideoForm::paintEvent(QPaintEvent *paint)
{
    Q_UNUSED(paint)
    QStyleOption opt;
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    opt.init(this);
#else
    opt.initFrom(this);
#endif
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void VideoForm::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)
    if (!isFullScreen() && this->show_toolbar) {
        QTimer::singleShot(500, this, [this](){
            showToolForm(this->show_toolbar);
        });
    }
}

void VideoForm::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    updateKeyMapOverlayGeometry();
    QSize goodSize = ui->keepRatioWidget->goodSize();
    if (goodSize.isEmpty()) {
        return;
    }
    QSize curSize = size();
    // 限制VideoForm尺寸不能小于keepRatioWidget good size
    if (m_widthHeightRatio > 1.0f) {
        // hor
        if (curSize.height() <= goodSize.height()) {
            setMinimumHeight(goodSize.height());
        } else {
            setMinimumHeight(0);
        }
    } else {
        // ver
        if (curSize.width() <= goodSize.width()) {
            setMinimumWidth(goodSize.width());
        } else {
            setMinimumWidth(0);
        }
    }
}

void VideoForm::updateKeyMapOverlayGeometry()
{
    if (!m_keyMapOverlay || !m_videoWidget) {
        return;
    }
    m_keyMapOverlay->setGeometry(m_videoWidget->geometry());
    m_keyMapOverlay->raise();
}

void VideoForm::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event)
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        grabCursor(false);
        return;
    }
    device->releaseAllTouches();
    grabCursor(false);
    Config::getInstance().setRect(device->getSerial(), geometry());
    device->disconnectDevice();
}

void VideoForm::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (device) {
        device->releaseAllTouches();
    }
}

void VideoForm::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void VideoForm::dragMoveEvent(QDragMoveEvent *event)
{
    Q_UNUSED(event)
}

void VideoForm::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event)
}

void VideoForm::dropEvent(QDropEvent *event)
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    const QMimeData *qm = event->mimeData();
    QList<QUrl> urls = qm->urls();

    for (const QUrl &url : urls) {
        QString file = url.toLocalFile();
        QFileInfo fileInfo(file);

        if (!fileInfo.exists()) {
            QMessageBox::warning(this, "QtScrcpy", tr("file does not exist"), QMessageBox::Ok);
            continue;
        }

        if (fileInfo.isFile() && fileInfo.suffix() == "apk") {
            emit device->installApkRequest(file);
            continue;
        }
        emit device->pushFileRequest(file, Config::getInstance().getPushFilePath() + fileInfo.fileName());
    }
}
