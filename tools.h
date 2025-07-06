#ifndef TOOLS_H
#define TOOLS_H

#include <QObject>
#include <QMouseEvent>
#include <QHash>

class ImageViewer;

enum class ToolType {
    None,
    AddLocator,
    DefineMeasurements,
    DefineWorldspace
};

class ITool : public QObject
{
    Q_OBJECT
public:
    explicit ITool(ImageViewer *viewer, QObject *parent = nullptr)
        : QObject(parent), m_viewer(viewer) {}
    virtual ~ITool() {}

    virtual void onMousePress(QMouseEvent *) {}
    virtual void onMouseMove(QMouseEvent *) {}
    virtual void onMouseRelease(QMouseEvent *) {}
    virtual void onEnter() {}
    virtual void onExit() {}

protected:
    ImageViewer *viewer() const { return m_viewer; }

private:
    ImageViewer *m_viewer;
};

class ToolController : public QObject
{
    Q_OBJECT
public:
    explicit ToolController(ImageViewer *viewer, QObject *parent = nullptr);

    void registerTool(ToolType type, ITool *tool);
    void setActiveTool(ToolType type);
    ToolType activeTool() const { return m_activeType; }

    void mousePressEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);

private:
    ImageViewer *m_viewer;
    QHash<ToolType, ITool*> m_tools;
    ITool *m_currentTool;
    ToolType m_activeType;
};

class AddLocatorTool : public ITool
{
    Q_OBJECT
public:
    using ITool::ITool;
    void onMousePress(QMouseEvent *event) override;
    void onEnter() override;
    void onExit() override;

signals:
    void locatorCreated(float x, float y);
};

class DefineMeasurementsTool : public ITool
{
    Q_OBJECT
public:
    using ITool::ITool;
};

class DefineWorldspaceTool : public ITool
{
    Q_OBJECT
public:
    using ITool::ITool;
};

#endif // TOOLS_H
