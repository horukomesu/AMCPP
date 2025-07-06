#include "tools.h"
#include "imageviewer.h"

ToolController::ToolController(ImageViewer *viewer, QObject *parent)
    : QObject(parent), m_viewer(viewer), m_currentTool(nullptr), m_activeType(ToolType::None)
{
}

void ToolController::registerTool(ToolType type, ITool *tool)
{
    if (m_tools.contains(type)) {
        delete m_tools[type];
    }
    m_tools[type] = tool;
}

void ToolController::setActiveTool(ToolType type)
{
    if (m_activeType == type)
        return;
    if (m_currentTool)
        m_currentTool->onExit();
    m_activeType = type;
    m_currentTool = m_tools.value(type, nullptr);
    if (m_currentTool)
        m_currentTool->onEnter();
}

void ToolController::mousePressEvent(QMouseEvent *e)
{
    if (m_currentTool)
        m_currentTool->onMousePress(e);
}

void ToolController::mouseMoveEvent(QMouseEvent *e)
{
    if (m_currentTool)
        m_currentTool->onMouseMove(e);
}

void ToolController::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_currentTool)
        m_currentTool->onMouseRelease(e);
}

// === AddLocatorTool ===

void AddLocatorTool::onMousePress(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (!viewer())
        return;
    QPointF pos = viewer()->mapToScene(event->pos());
    QRectF rect = viewer()->sceneRect();
    if (rect.width() <= 0 || rect.height() <= 0)
        return;
    float x = pos.x() / rect.width();
    float y = pos.y() / rect.height();
    emit locatorCreated(x, y);
    event->accept();
}

void AddLocatorTool::onEnter()
{
    if (viewer())
        viewer()->setCursor(Qt::CrossCursor);
}

void AddLocatorTool::onExit()
{
    if (viewer())
        viewer()->unsetCursor();
}


// === DefineMeasurementsTool (placeholder) ===

// Currently no specific behaviour

// === DefineWorldspaceTool (placeholder) ===

// Currently no specific behaviour

