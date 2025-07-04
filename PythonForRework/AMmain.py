"""Main entry point for MyImageModelerPlugin.

Этот скрипт загружает Qt UI, созданный в ``AMUI.ui`` как QMainWindow, и вешает обработчики.
Используется ТОЛЬКО внутри Autodesk 3ds Max 2023+.
"""
import sys
import os
import json
from pathlib import Path

from PyQt6 import QtWidgets, QtGui, uic, QtCore

from PyQt6.QtGui import QPainter, QPixmap, QImage, QColor, QPen, QIcon, QAction, QKeySequence, QShortcut
from PyQt6.QtCore import Qt, pyqtSignal, QFile, QPointF, QPoint, QEvent, QObject
from PyQt6.QtWidgets import QGraphicsItem


QFileDialog = QtWidgets.QFileDialog
QMessageBox = QtWidgets.QMessageBox


import numpy as np
import importlib
import site
print(site.getusersitepackages())

BASE_DIR = os.path.dirname(__file__)
sys.path.insert(0, BASE_DIR)

import AMUtilities
importlib.reload(AMUtilities)

SETTINGS_FILE = Path(__file__).parent / "settings.json"

import CameraCalibrator
importlib.reload(CameraCalibrator)
from CameraCalibrator import CameraCalibrator

def load_settings() -> dict:
    if SETTINGS_FILE.exists():
        with open(SETTINGS_FILE, "r", encoding="utf-8 ") as f:
            try:
                return json.load(f)
            except json.JSONDecodeError:
                return {}
    return {}

def save_settings(data: dict) -> None:
    with open(SETTINGS_FILE, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)

settings_data = load_settings()
MAX_RECENT_PROJECTS = 5
scene_file_path = None

class ImageViewer(QtWidgets.QGraphicsView):
    """Interactive viewer placed inside ``MainFrame``."""

    locator_added = pyqtSignal(float, float)

    navigate = pyqtSignal(int)  # emit +1/-1 to switch images

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setScene(QtWidgets.QGraphicsScene(self))
        self._pixmap = QtWidgets.QGraphicsPixmapItem()
        self.scene().addItem(self._pixmap)
        self.mark_items = []

        self.setRenderHints(
            QPainter.RenderHint.Antialiasing | QPainter.RenderHint.SmoothPixmapTransform
)
        self.setDragMode(QtWidgets.QGraphicsView.DragMode.NoDrag)

        self.setTransformationAnchor(QtWidgets.QGraphicsView.ViewportAnchor.NoAnchor)
        self.setResizeAnchor(QtWidgets.QGraphicsView.ViewportAnchor.NoAnchor)

        self._panning = False
        self._pan_start = QtCore.QPoint()
        self.adding_locator = False
        self.zoom_step = 1.2

        self.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)

        self.customContextMenuRequested.connect(self._show_action_menu)

        self._action_menu = None

    def load_image(self, qimg: QtGui.QImage, keep_transform: bool = False):
        current_transform = self.transform()
        h_val = self.horizontalScrollBar().value()
        v_val = self.verticalScrollBar().value()

        self.scene().setSceneRect(0, 0, qimg.width(), qimg.height())
        self._pixmap.setPixmap(QtGui.QPixmap.fromImage(qimg))

        if keep_transform:
            self.setTransform(current_transform)
            self.horizontalScrollBar().setValue(h_val)
            self.verticalScrollBar().setValue(v_val)
        else:
            self.resetTransform()
            self.fitInView(self.sceneRect(), QtCore.Qt.AspectRatioMode.KeepAspectRatio)

    def set_markers(self, markers, highlight_name=None):
        for item in self.mark_items:
            self.scene().removeItem(item)
        self.mark_items = []
        for m in markers:
            self._add_marker_item(
                m["x"],
                m["y"],
                m.get("name", ""),
                m.get("name") == highlight_name,
                m.get("error")
            )

    def _add_marker_item(self, x_norm: float, y_norm: float, name: str, highlight: bool = False, error: float = 0.0):
        color = AMUtilities.error_to_color(error or 0.0)
        pen = QtGui.QPen(color)
        pen.setWidth(2 if highlight else 1)
        pos = QtCore.QPointF(x_norm * self.sceneRect().width(), y_norm * self.sceneRect().height())

        line1 = QtWidgets.QGraphicsLineItem(-5, 0, 5, 0)
        line2 = QtWidgets.QGraphicsLineItem(0, -5, 0, 5)
        for line in (line1, line2):
            line.setPen(pen)
            line.setPos(pos)
            line.setFlag(QtWidgets.QGraphicsItem.GraphicsItemFlag.ItemIgnoresTransformations)

            self.scene().addItem(line)
            self.mark_items.append(line)

        text = QtWidgets.QGraphicsSimpleTextItem(name)
        text.setFlag(QtWidgets.QGraphicsItem.GraphicsItemFlag.ItemIgnoresTransformations)
        text.setPos(pos + QtCore.QPointF(6, -6))
        self.scene().addItem(text)
        self.mark_items.append(text)

    def mousePressEvent(self, event):
        if self.adding_locator:
            if event.button() == QtCore.Qt.MouseButton.LeftButton:
                pos = self.mapToScene(event.pos())
                x_norm = pos.x() / self.sceneRect().width()
                y_norm = pos.y() / self.sceneRect().height()
                self.locator_added.emit(x_norm, y_norm)
                return

        if event.button() == QtCore.Qt.MouseButton.MiddleButton:
            self._panning = True
            self._pan_start = event.pos()
            self.setCursor(QtCore.Qt.ClosedHandCursor)
        else:
            super().mousePressEvent(event)

    def mouseMoveEvent(self, event):
        if self._panning:
            delta = event.pos() - self._pan_start
            self._pan_start = event.pos()
            self.horizontalScrollBar().setValue(
                self.horizontalScrollBar().value() - delta.x()
            )
            self.verticalScrollBar().setValue(
                self.verticalScrollBar().value() - delta.y()
            )
        else:
            super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event):
        if event.button() == QtCore.Qt.MouseButton.MiddleButton and self._panning:
            self._panning = False
            self.setCursor(QtCore.Qt.CursorShape.ArrowCursor)
        else:
            super().mouseReleaseEvent(event)

    def wheelEvent(self, event):
        if event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            step = 1 if event.angleDelta().y() > 0 else -1
            self.navigate.emit(step)
            return

        old_pos = self.mapToScene(event.position().toPoint())
        zoom_out = event.angleDelta().y() < 0
        factor = 1.0 / self.zoom_step if zoom_out else self.zoom_step
        self.scale(factor, factor)
        new_pos = self.mapToScene(event.position().toPoint())
        delta = new_pos - old_pos
        self.translate(delta.x(), delta.y())

    def mouseDoubleClickEvent(self, event):
        if self._pixmap.pixmap() and not self._pixmap.pixmap().isNull():
            self.fitInView(self.sceneRect(), QtCore.Qt.AspectRatioMode.KeepAspectRatio)
        super().mouseDoubleClickEvent(event)

    def _show_action_menu(self, pos):
        if self._action_menu is None:
            self._action_menu = QtWidgets.QMenu(self)
            self._action_menu.addAction("Add Locator", add_locator)
            self._action_menu.addAction("Calibrate", calibrate)
            self._action_menu.addAction("Define Worldspace", define_worldspace)
            self._action_menu.addAction(
                "Define Reference Distance", define_reference_distance
            )
            self._action_menu.addAction(
                "Add Modeling Locator", add_modeling_locator
            )
        self._action_menu.exec(self.mapToGlobal(pos))



def get_image_markers(index: int):
    """Return list of markers for the given image index."""
    markers = []
    for loc in getattr(main_window, "locators", []):
        pos = loc.get("positions", {}).get(index)
        if pos:
            markers.append({
                "name": loc["name"],
                "x": pos["x"],
                "y": pos["y"],
                "error": loc.get("error")
            })
    return markers


def update_tree():
    """Rebuild the tree with image and locator nodes."""
    tree = main_window.MainTree
    tree.clear()

    img_root = QtWidgets.QTreeWidgetItem(tree, ["Images"])
    img_root.setData(0, QtCore.Qt.ItemDataRole.UserRole, ("images_root",))
    img_errors = getattr(main_window, "image_errors", {})
    for idx, path in enumerate(main_window.image_paths):
        item = QtWidgets.QTreeWidgetItem(img_root, [Path(path).name])
        item.setData(0, QtCore.Qt.ItemDataRole.UserRole, ("image", idx))
        err = img_errors.get(idx)
        if err is not None:
            color = AMUtilities.error_to_color(err)
            icon = QtGui.QPixmap(16, 16)
            icon.fill(color)
            item.setIcon(0, QtGui.QIcon(icon))

    loc_root = QtWidgets.QTreeWidgetItem(tree, ["Locators"])
    loc_root.setData(0, QtCore.Qt.ItemDataRole.UserRole, ("loc_root",))
    for loc in getattr(main_window, "locators", []):
        item = QtWidgets.QTreeWidgetItem(loc_root, [loc["name"]])
        item.setData(0, QtCore.Qt.ItemDataRole.UserRole, ("locator", loc["name"]))
        color = AMUtilities.error_to_color(loc.get("error", 0.0))
        icon = QtGui.QPixmap(16, 16)
        icon.fill(color)
        item.setIcon(0, QtGui.QIcon(icon))

    img_root.setExpanded(True)
    loc_root.setExpanded(True)


def get_next_locator_name() -> str:
    """Return the minimal free locator name as ``locN``."""
    existing = {loc["name"] for loc in getattr(main_window, "locators", [])}
    idx = 1
    while f"loc{idx}" in existing:
        idx += 1
    return f"loc{idx}"


def exit_locator_mode():
    """Reset locator placement mode."""
    main_window.locator_mode = False
    main_window.viewer.adding_locator = False
    main_window.viewer.setCursor(QtCore.Qt.CursorShape.ArrowCursor)


def show_image(index: int, keep_view: bool = False):
    if 0 <= index < len(main_window.images):
        main_window.current_image_index = index
        main_window.viewer.load_image(main_window.images[index], keep_transform=keep_view)
        highlight = getattr(main_window, "selected_locator", None)
        main_window.viewer.set_markers(get_image_markers(index), highlight)


def next_image():
    """Switch to the next image cyclically while keeping the current view."""
    if not main_window.images:
        return
    idx = (getattr(main_window, "current_image_index", 0) + 1) % len(main_window.images)
    show_image(idx, keep_view=True)


def prev_image():
    """Switch to the previous image cyclically while keeping the current view."""
    if not main_window.images:
        return
    idx = (getattr(main_window, "current_image_index", 0) - 1) % len(main_window.images)
    show_image(idx, keep_view=True)


def on_tree_selection_changed(current, _previous):
    if not current:
        return
    data = current.data(0, QtCore.Qt.ItemDataRole.UserRole)
    if not data:
        return
    if data[0] == "image":
        main_window.selected_locator = None
        exit_locator_mode()
        show_image(data[1])
    elif data[0] == "locator":
        exit_locator_mode()
        main_window.selected_locator = data[1]
        main_window.viewer.adding_locator = True
        main_window.viewer.setCursor(QtCore.Qt.CursorShape.CrossCursor)
        main_window.locator_mode = True
        show_image(getattr(main_window, "current_image_index", 0), keep_view=True)


def delete_selected_locator():
    item = main_window.MainTree.currentItem()
    if not item:
        return
    data = item.data(0, QtCore.Qt.ItemDataRole.UserRole)
    if not data or data[0] != "locator":
        return
    name = data[1]
    main_window.locators = [l for l in main_window.locators if l["name"] != name]
    if main_window.selected_locator == name:
        main_window.selected_locator = None
    update_tree()
    show_image(getattr(main_window, "current_image_index", 0), keep_view=True)


def on_locator_added(x_norm: float, y_norm: float):
    idx = getattr(main_window, "current_image_index", 0)
    if idx >= len(main_window.image_paths):
        return
    name = getattr(main_window, "selected_locator", None)
    if not name:
        return
    loc = next((l for l in main_window.locators if l["name"] == name), None)
    if not loc:
        return
    loc.setdefault("positions", {})[idx] = {"x": x_norm, "y": y_norm}
    update_tree()
    show_image(idx, keep_view=True)

def update_window_title():
    base_name = "AutoModeler"
    if scene_file_path:
        name = Path(scene_file_path).stem  # без .ams
    else:
        name = "Untitled"
    main_window.setWindowTitle(f"{name} - {base_name}")




def new_scene():
    global scene_file_path
    exit_locator_mode()
    main_window.image_paths = []
    main_window.images = []
    main_window.locators = []
    main_window.selected_locator = None
    main_window.image_errors = {}
    scene_file_path = None  # ← сброс пути
    update_window_title()
    update_tree()
    main_window.viewer._pixmap.setPixmap(QtGui.QPixmap())
    main_window.viewer.set_markers([])

def import_images():
    exit_locator_mode()
    paths, _ = QtWidgets.QFileDialog.getOpenFileNames(
        main_window, "Select Images", "", "Images (*.png *.jpg *.jpeg *.tif)"
    )
    paths = AMUtilities.verify_paths(paths)
    main_window.image_paths = paths
    main_window.images = AMUtilities.load_images(paths)
    main_window.locators = []
    main_window.selected_locator = None
    main_window.image_errors = {}
    update_tree()
    if main_window.images:
        show_image(0)
    else:
        main_window.viewer._pixmap.setPixmap(QtGui.QPixmap())
        main_window.viewer.set_markers([])

def save_scene():
    global scene_file_path
    if not scene_file_path:
        save_scene_as()
        return
    try:
        AMUtilities.save_scene(
            path=scene_file_path,
            image_paths=main_window.image_paths,
            locators=main_window.locators
        )
    except Exception as e:
        QtWidgets.QMessageBox.critical(main_window, "Save Failed", str(e))




def save_scene_as():
    global scene_file_path
    path, _ = QtWidgets.QFileDialog.getSaveFileName(
        main_window, "Save Scene As", scene_file_path or "", "AMS Scene (*.ams)"
    )
    if not path:
        return

    # Гарантируем .ams
    if not path.lower().endswith(".ams"):
        path += ".ams"

    scene_file_path = path
    update_window_title()
    save_scene()




def load_scene():
    global scene_file_path
    path, _ = QtWidgets.QFileDialog.getOpenFileName(
        parent=main_window,
        caption="Load Scene",
        directory="",
        filter="AMS or RZI Scene (*.ams *.rzi)"
    )
    if not path:
        return
    try:
        data = AMUtilities.load_scene_any(path)
        main_window.image_paths = data.get("image_paths", [])
        main_window.locators = data.get("locators", [])
        main_window.images = AMUtilities.load_images(main_window.image_paths)
        main_window.selected_locator = None
        main_window.image_errors = {}

        # Обнуляем путь, если .rzi — мы не сохраняем rzi, только .ams
        if Path(path).suffix.lower() == ".ams":
            scene_file_path = path
        else:
            scene_file_path = None  # → Save вызовет Save As и сохранит как .ams

        update_window_title()
        update_tree()
        if main_window.images:
            show_image(0)
        else:
            main_window.viewer._pixmap.setPixmap(QtGui.QPixmap())
            main_window.viewer.set_markers([])
    
        add_to_recent(path)


    except Exception as e:
        import traceback
        QtWidgets.QMessageBox.critical(
            main_window, "Load Failed", f"{e}\n{traceback.format_exc()}"
        )
        
def add_to_recent(path: str):
    if not path.lower().endswith(".ams"):
        return

    path = str(path).strip()
    if not path:
        return

    recent = settings_data.get("recent_projects", [])
    if path in recent:
        recent.remove(path)
    recent.insert(0, path)
    recent = recent[:MAX_RECENT_PROJECTS]
    settings_data["recent_projects"] = recent
    save_settings(settings_data)
    update_recent_projects_menu()


def make_recent_action(path: str) -> QAction:
    action = QAction(str(path), main_window)
    action.triggered.connect(lambda checked=False, p=path: open_recent_project(p))
    return action




def update_recent_projects_menu():
    menu = main_window.menuRecent_Projects
    menu.clear()

    recent_paths = settings_data.get("recent_projects", [])
    valid_paths = []

    for p in recent_paths:
        try:
            p_str = str(p).strip()
            if p_str and Path(p_str).is_file():
                valid_paths.append(p_str)
        except Exception:
            continue

    for path in valid_paths:
        action = QAction(path, main_window)
        action.triggered.connect(lambda checked=False, p=path: open_recent_project(p))
        menu.addAction(action)



def open_recent_project(path: str):
    if not Path(path).is_file():
        QtWidgets.QMessageBox.warning(main_window, "File Not Found", f"File not found:\n{path}")
        return

    try:
        data = AMUtilities.load_scene_any(path)
        main_window.image_paths = data.get("image_paths", [])
        main_window.locators = data.get("locators", [])
        main_window.images = AMUtilities.load_images(main_window.image_paths)
        main_window.selected_locator = None
        main_window.image_errors = {}

        global scene_file_path
        scene_file_path = path
        update_window_title()
        update_tree()
        if main_window.images:
            show_image(0)
        else:
            main_window.viewer._pixmap.setPixmap(QtGui.QPixmap())
            main_window.viewer.set_markers([])

        add_to_recent(path)


    except Exception as e:
        import traceback
        QtWidgets.QMessageBox.critical(
            main_window, "Load Failed", f"{e}\n{traceback.format_exc()}"
        )





def preferences():
    QtWidgets.QMessageBox.information(main_window, "Preferences", "Preferences dialog not implemented.")

def undo():
    QtWidgets.QMessageBox.information(main_window, "Undo", "Undo not implemented.")

def redo():
    QtWidgets.QMessageBox.information(main_window, "Redo", "Redo not implemented.")

def add_locator():
    if not main_window.images:
        QtWidgets.QMessageBox.warning(main_window, "Add Locator", "No image loaded.")
        return
    exit_locator_mode()
    name = get_next_locator_name()
    loc = {"name": name, "positions": {}}
    main_window.locators.append(loc)
    update_tree()
    main_window.selected_locator = name
    main_window.viewer.adding_locator = True
    main_window.locator_mode = True
    main_window.viewer.setCursor(QtCore.Qt.CursorShape.CrossCursor)
    # select item in tree
    items = main_window.MainTree.findItems(name, QtCore.Qt.MatchFlag.MatchRecursive)
    if items:
        main_window.MainTree.setCurrentItem(items[0])


def calibrate():
    """Calibrate cameras using manually placed locators."""
    if not getattr(main_window, "image_paths", []):
        QtWidgets.QMessageBox.warning(
            main_window, "Calibrate", "No images loaded for calibration."
        )
        return

    calibrator = CameraCalibrator()
    try:
        calibrator.load_images(main_window.image_paths)

        # ---- Новое: собрать point_data из locators ----
        point_data = {}
        for set_id, loc in enumerate(getattr(main_window, "locators", [])):
            for img_idx, pos in loc.get("positions", {}).items():
                if set_id not in point_data:
                    point_data[set_id] = {}
                # Преобразовать к целочисленному индексу, если ключ-строка
                try:
                    img_idx_int = int(img_idx)
                except Exception:
                    img_idx_int = img_idx
                point_data[set_id][img_idx_int] = (pos["x"] * main_window.images[img_idx_int].width(), 
                                                   pos["y"] * main_window.images[img_idx_int].height())
        calibrator.load_point_data(point_data)
        success = calibrator.calibrate()
        if not success:
            QtWidgets.QMessageBox.critical(
                main_window, "Calibrate", f"Calibration failed. Check your points."
            )
            return
    except Exception as exc:
        import traceback
        QtWidgets.QMessageBox.critical(
            main_window, "Calibrate", f"Calibration failed:\n{exc}\n{traceback.format_exc()}"
        )
        return

    main_window.calibration = calibrator
    err_dict = calibrator.get_reprojection_error() or {}
    for idx, loc in enumerate(getattr(main_window, "locators", [])):
        loc["error"] = err_dict.get(str(idx), None)
    main_window.image_errors = calibrator.get_reprojection_errors_per_image() or {}

    msg = (
        f"Recovered {len(calibrator.get_camera_parameters())} cameras.\n"
        f"3D points: {len(calibrator.get_points_3d())}"
    )
    if err_dict:
        avg_err = sum(err_dict.values()) / len(err_dict)
        msg += f"\nReprojection error: {avg_err:.4f}"
    if main_window.image_errors:
        avg_img_err = sum(main_window.image_errors.values()) / len(main_window.image_errors)
        msg += f"\nImage error: {avg_img_err:.4f}"

    QtWidgets.QMessageBox.information(main_window, "Calibration Completed", msg)
    update_tree()
    show_image(getattr(main_window, "current_image_index", 0), keep_view=True)



def define_worldspace():
    QtWidgets.QMessageBox.information(main_window, "Worldspace", "Define worldspace not implemented.")

def define_reference_distance():
    QtWidgets.QMessageBox.information(main_window, "Reference Distance", "Define reference distance not implemented.")

def add_modeling_locator():
    QtWidgets.QMessageBox.information(main_window, "Modeling Locator", "Add modeling locator not implemented.")

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    main_window = uic.loadUi("AMUI.ui")

    # Поле инициализации
    main_window.image_paths = []
    main_window.images = []
    main_window.locators = []
    main_window.selected_locator = None
    main_window.locator_mode = False
    main_window.image_errors = {}

    # Viewer init
    layout = QtWidgets.QVBoxLayout(main_window.MainFrame)
    layout.setContentsMargins(0, 0, 0, 0)
    main_window.viewer = ImageViewer(main_window.MainFrame)
    main_window.viewer.locator_added.connect(on_locator_added)
    main_window.viewer.navigate.connect(lambda step: (next_image() if step > 0 else prev_image()))
    layout.addWidget(main_window.viewer)

    # Tree events
    main_window.MainTree.currentItemChanged.connect(on_tree_selection_changed)

    class _DeleteFilter(QtCore.QObject):
        def eventFilter(self, obj, event):
            if event.type() == QEvent.Type.KeyPress and event.key() == QtCore.Qt.Key.Key_Delete:
                delete_selected_locator()
                return True
            return super().eventFilter(obj, event)

    main_window._del_filter = _DeleteFilter(main_window.MainTree)
    main_window.MainTree.installEventFilter(main_window._del_filter)

    class _EscFilter(QtCore.QObject):
        def eventFilter(self, obj, event):
            if event.type() == QEvent.Type.KeyPress and event.key() == QtCore.Qt.Key.Key_Escape:
                exit_locator_mode()
                return True
            return super().eventFilter(obj, event)

    main_window._esc_filter = _EscFilter(main_window.viewer)
    main_window.viewer.installEventFilter(main_window._esc_filter)

    QShortcut(QKeySequence(Qt.Key.Key_Right), main_window).activated.connect(next_image)
    QShortcut(QKeySequence(Qt.Key.Key_Left), main_window).activated.connect(prev_image)

    # Menu / Buttons
    main_window.btnAddLoc.clicked.connect(add_locator)
    main_window.btnCalibrate.clicked.connect(calibrate)
    main_window.btnDFWS.clicked.connect(define_worldspace)
    main_window.btnDFMM.clicked.connect(define_reference_distance)
    main_window.btnLocMod.clicked.connect(add_modeling_locator)

    main_window.actionNEW.triggered.connect(new_scene)
    main_window.actionOpen.triggered.connect(load_scene)
    main_window.actionSave.triggered.connect(save_scene)
    main_window.actionSave_As.triggered.connect(save_scene_as)
    main_window.actionLoad.triggered.connect(import_images)
    main_window.actionPreferences.triggered.connect(preferences)
    main_window.actionUndo.triggered.connect(undo)
    main_window.actionRedo.triggered.connect(redo)

    main_window.setWindowTitle("AutoModeler")
    main_window.resize(1200, 900)
    main_window.show()
    update_recent_projects_menu()

    sys.exit(app.exec())

