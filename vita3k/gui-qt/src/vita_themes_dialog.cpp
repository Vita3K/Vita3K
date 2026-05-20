// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <gui-qt/vita_themes_dialog.h>

#include <gui-qt/gui_settings.h>
#include <gui-qt/qt_utils.h>
#include <gui-qt/theme_manager.h>

#include <util/log.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const QSize k_default_preview_aspect(960, 544);

QString to_qstring(const std::string &value) {
    return QString::fromStdString(value);
}

QString background_source_label(const gui::VitaThemeBackgroundOption &background) {
    return (background.source == gui::VitaThemeBackgroundSource::Lockscreen)
        ? QObject::tr("Lockscreen")
        : QObject::tr("Home");
}

QLabel *make_section_label(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setProperty("themeRole", QStringLiteral("sectionHeader"));
    gui::utils::refresh_theme_state(label);
    return label;
}

class AspectRatioPreviewLabel final : public QLabel {
public:
    explicit AspectRatioPreviewLabel(QWidget *parent = nullptr)
        : QLabel(parent) {
    }

    bool hasHeightForWidth() const override {
        return true;
    }

    int heightForWidth(int width) const override {
        const int safe_width = std::max(width, minimumSizeHint().width());
        return std::max(
            minimumSizeHint().height(),
            (safe_width * m_aspect_size.height() + (m_aspect_size.width() / 2)) / m_aspect_size.width());
    }

    QSize sizeHint() const override {
        return minimumSizeHint();
    }

    QSize minimumSizeHint() const override {
        return QSize(420, 236);
    }

    void set_aspect_size(const QSize &aspect_size) {
        const QSize normalized = aspect_size.expandedTo(QSize(1, 1));
        if (m_aspect_size == normalized)
            return;

        m_aspect_size = normalized;
        updateGeometry();
    }

private:
    QSize m_aspect_size = k_default_preview_aspect;
};

} // namespace

VitaThemesDialog::VitaThemesDialog(
    std::shared_ptr<GuiSettings> gui_settings,
    ThemeManager &theme_manager,
    QWidget *parent)
    : QDialog(parent)
    , m_gui_settings(std::move(gui_settings))
    , m_theme_manager(theme_manager) {
    setObjectName(QStringLiteral("vita_themes_dialog"));
    setWindowTitle(tr("Vita Themes"));
    setAttribute(Qt::WA_DeleteOnClose, false);

    build_ui();
    restore_persistent_state();
    reload();
}

void VitaThemesDialog::reload() {
    m_theme_manager.refresh_vita_theme_catalog();
    const auto preferred_selection = m_theme_manager.applied_vita_theme_selection();
    const QString preferred_theme_id = preferred_selection
        ? preferred_selection->theme_id
        : QString();
    repopulate_theme_list(preferred_theme_id);
}

void VitaThemesDialog::closeEvent(QCloseEvent *event) {
    save_persistent_state();
    QDialog::closeEvent(event);
}

void VitaThemesDialog::resizeEvent(QResizeEvent *event) {
    QDialog::resizeEvent(event);
    update_preview_pixmap();
}

bool VitaThemesDialog::eventFilter(QObject *watched, QEvent *event) {
    if (!watched || !event)
        return QDialog::eventFilter(watched, event);

    if ((watched == m_preview || watched == m_package_thumbnail)
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        if (watched == m_preview)
            update_preview_pixmap();
        else
            update_package_thumbnail();
    }

    if (event->type() == QEvent::Enter || event->type() == QEvent::FocusIn) {
        if (auto *widget = qobject_cast<QWidget *>(watched)) {
            const QString description = widget->property("_desc").toString();
            if (!description.isEmpty())
                set_description(widget->property("_desc_title").toString(), description);
        }
    } else if (event->type() == QEvent::Leave || event->type() == QEvent::FocusOut) {
        if (auto *widget = qobject_cast<QWidget *>(watched)) {
            if (!widget->property("_desc").toString().isEmpty())
                reset_description();
        }
    }

    return QDialog::eventFilter(watched, event);
}

void VitaThemesDialog::build_ui() {
    auto *root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(6, 6, 6, 6);
    root_layout->setSpacing(6);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("vitaThemesSplitter"));
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(10);
    splitter->setStyleSheet(QStringLiteral(
        "QSplitter#vitaThemesSplitter::handle { background: transparent; }"));
    root_layout->addWidget(splitter, 1);

    auto *left_panel = new QWidget(splitter);
    auto *left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(6);

    auto *left_header = make_section_label(tr("Installed Themes"), left_panel);
    left_layout->addWidget(left_header);

    m_theme_list = new QListWidget(left_panel);
    m_theme_list->setObjectName(QStringLiteral("vitaThemesList"));
    m_theme_list->setSelectionMode(QAbstractItemView::SingleSelection);
    left_layout->addWidget(m_theme_list, 1);
    register_description(
        m_theme_list,
        tr("Installed Themes"),
        tr("Select an installed Vita theme to preview it, choose a primary background, and prepare a generated emulator theme."));

    auto *card = new QWidget(left_panel);
    auto *card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(0, 0, 0, 0);
    card_layout->setSpacing(4);

    m_package_thumbnail = new QLabel(card);
    m_package_thumbnail->setAlignment(Qt::AlignCenter);
    m_package_thumbnail->setMinimumHeight(96);
    m_package_thumbnail->setMaximumHeight(96);
    m_package_thumbnail->setFrameShape(QFrame::StyledPanel);
    m_package_thumbnail->installEventFilter(this);
    card_layout->addWidget(m_package_thumbnail);

    m_theme_title = new QLabel(card);
    m_theme_title->setWordWrap(true);
    card_layout->addWidget(m_theme_title);

    m_theme_provider = new QLabel(card);
    m_theme_provider->setWordWrap(true);
    m_theme_provider->setProperty("themeRole", QStringLiteral("mutedText"));
    gui::utils::refresh_theme_state(m_theme_provider);
    card_layout->addWidget(m_theme_provider);
    left_layout->addWidget(card);

    auto *right_panel = new QWidget(splitter);
    auto *right_layout = new QVBoxLayout(right_panel);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(6);

    auto *preview_card = new QWidget(right_panel);
    preview_card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *preview_layout = new QVBoxLayout(preview_card);
    preview_layout->setContentsMargins(0, 0, 0, 0);
    preview_layout->setSpacing(4);

    preview_layout->addWidget(make_section_label(tr("Preview"), preview_card));

    auto *preview_content = new QWidget(preview_card);
    preview_content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *preview_content_layout = new QHBoxLayout(preview_content);
    preview_content_layout->setContentsMargins(0, 0, 0, 0);
    preview_content_layout->setSpacing(6);

    auto *preview_stage = new QWidget(preview_content);
    preview_stage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *preview_stage_layout = new QVBoxLayout(preview_stage);
    preview_stage_layout->setContentsMargins(0, 0, 0, 0);
    preview_stage_layout->setSpacing(0);
    preview_stage_layout->addStretch(1);

    m_preview = new AspectRatioPreviewLabel(preview_stage);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_preview->setFrameShape(QFrame::StyledPanel);
    m_preview->setText(tr("No preview available"));
    preview_stage_layout->addWidget(m_preview);
    preview_stage_layout->addStretch(1);
    preview_content_layout->addWidget(preview_stage, 1);
    register_description(
        m_preview,
        tr("Preview"),
        tr("Shows the currently previewed Vita background. Select a row in the list to compare slides, or use Primary to choose which one gets applied."));

    auto *backgrounds_side = new QWidget(preview_content);
    backgrounds_side->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    backgrounds_side->setMinimumWidth(288);
    backgrounds_side->setMaximumWidth(360);
    auto *backgrounds_layout = new QVBoxLayout(backgrounds_side);
    backgrounds_layout->setContentsMargins(0, 0, 0, 0);
    backgrounds_layout->setSpacing(4);

    backgrounds_layout->addWidget(make_section_label(tr("Slides"), backgrounds_side));

    auto *cycle_row = new QWidget(backgrounds_side);
    auto *cycle_layout = new QHBoxLayout(cycle_row);
    cycle_layout->setContentsMargins(0, 0, 0, 0);
    cycle_layout->setSpacing(4);

    m_cycle_checkbox = new QCheckBox(cycle_row);
    cycle_layout->addWidget(m_cycle_checkbox, 0, Qt::AlignVCenter);
    register_description(
        m_cycle_checkbox,
        tr("Cycle Backgrounds"),
        tr("Rotate through the backgrounds checked in the list below. The primary background always remains part of the cycle order."));

    auto *cycle_label = new QLabel(tr("Cycle"), cycle_row);
    cycle_layout->addWidget(cycle_label, 0, Qt::AlignVCenter);

    auto *cycle_interval_label = new QLabel(tr("Every"), cycle_row);
    cycle_layout->addWidget(cycle_interval_label, 0, Qt::AlignVCenter);

    m_cycle_interval_spinbox = new QSpinBox(cycle_row);
    m_cycle_interval_spinbox->setRange(5, 120);
    m_cycle_interval_spinbox->setSuffix(tr(" s"));
    m_cycle_interval_spinbox->setValue(15);
    m_cycle_interval_spinbox->setFixedWidth(70);
    cycle_layout->addWidget(m_cycle_interval_spinbox, 0, Qt::AlignVCenter);
    register_description(
        m_cycle_interval_spinbox,
        tr("Cycle Interval"),
        tr("Choose how long each selected background stays on screen before the next fade begins."));

    cycle_layout->addStretch(1);

    m_cycle_summary_value = new QLabel(cycle_row);
    m_cycle_summary_value->setProperty("themeRole", QStringLiteral("mutedText"));
    m_cycle_summary_value->setMinimumWidth(68);
    gui::utils::refresh_theme_state(m_cycle_summary_value);
    cycle_layout->addWidget(m_cycle_summary_value, 0, Qt::AlignVCenter);
    backgrounds_layout->addWidget(cycle_row);

    m_background_list = new QTreeWidget(backgrounds_side);
    m_background_list->setObjectName(QStringLiteral("vitaThemesBackgroundList"));
    m_background_list->setColumnCount(3);
    m_background_list->setHeaderLabels({ tr("Background"), tr("Primary"), tr("Cycle") });
    m_background_list->setRootIsDecorated(false);
    m_background_list->setItemsExpandable(false);
    m_background_list->setIndentation(0);
    m_background_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_background_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_background_list->setAlternatingRowColors(false);
    m_background_list->setUniformRowHeights(false);
    m_background_list->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_background_list->setMinimumHeight(212);
    if (auto *header = m_background_list->header()) {
        header->setStretchLastSection(false);
        header->setSectionResizeMode(0, QHeaderView::Stretch);
        header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    }
    backgrounds_layout->addWidget(m_background_list, 1);
    register_description(
        m_background_list,
        tr("Slides"),
        tr("Select a slide to preview it. Use the Primary column to choose the applied background and the Cycle column to decide which slides rotate."));

    connect(m_background_list, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (!m_background_list)
            return;
        if (auto *item = m_background_list->currentItem())
            set_preview_background(item->data(0, Qt::UserRole).toString());
    });

    preview_content_layout->addWidget(backgrounds_side);
    preview_layout->addWidget(preview_content);
    right_layout->addWidget(preview_card);

    auto *appearance_card = new QGroupBox(tr("Appearance"), right_panel);
    appearance_card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *appearance_layout = new QVBoxLayout(appearance_card);
    appearance_layout->setContentsMargins(8, 8, 8, 8);
    appearance_layout->setSpacing(4);

    auto *readability_row = new QWidget(appearance_card);
    auto *readability_layout = new QHBoxLayout(readability_row);
    readability_layout->setContentsMargins(0, 0, 0, 0);
    readability_layout->setSpacing(6);

    auto *readability_label = new QLabel(tr("Background Opaqueness"), readability_row);
    readability_layout->addWidget(readability_label);

    m_readability_slider = new QSlider(Qt::Horizontal, readability_row);
    m_readability_slider->setRange(0, 100);
    m_readability_slider->setValue(55);
    readability_layout->addWidget(m_readability_slider, 1);
    register_description(
        m_readability_slider,
        tr("Background Opaqueness"),
        tr("Adjust how strongly the generated theme dims the background behind Vita shell panels for readability."));

    m_readability_value = new QLabel(readability_row);
    readability_layout->addWidget(m_readability_value);
    appearance_layout->addWidget(readability_row);

    m_normalize_fonts_checkbox = new QCheckBox(tr("Normalize Font Colors"), appearance_card);
    m_normalize_fonts_checkbox->setChecked(true);
    appearance_layout->addWidget(m_normalize_fonts_checkbox);
    register_description(
        m_normalize_fonts_checkbox,
        tr("Normalize Font Colors"),
        tr("Use a more reliable light or dark text palette instead of the original Vita theme font colors when generating the stylesheet."));
    right_layout->addWidget(appearance_card);

    auto *meta_card = new QGroupBox(tr("Details"), right_panel);
    meta_card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *meta_card_layout = new QVBoxLayout(meta_card);
    meta_card_layout->setContentsMargins(8, 8, 8, 8);
    meta_card_layout->setSpacing(4);

    auto *meta_layout = new QFormLayout();
    meta_layout->setContentsMargins(0, 0, 0, 0);
    meta_layout->setHorizontalSpacing(20);
    meta_layout->setVerticalSpacing(6);
    meta_layout->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    meta_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_version_value = new QLabel(meta_card);
    m_version_value->setWordWrap(true);
    meta_layout->addRow(tr("Version"), m_version_value);

    m_theme_id_value = new QLabel(meta_card);
    m_theme_id_value->setWordWrap(true);
    meta_layout->addRow(tr("Theme ID"), m_theme_id_value);

    m_size_value = new QLabel(meta_card);
    m_size_value->setWordWrap(true);
    meta_layout->addRow(tr("Installed Size"), m_size_value);

    m_updated_value = new QLabel(meta_card);
    m_updated_value->setWordWrap(true);
    meta_layout->addRow(tr("Updated"), m_updated_value);

    m_background_value = new QLabel(meta_card);
    m_background_value->setWordWrap(true);
    meta_layout->addRow(tr("Background"), m_background_value);

    m_cycle_value = new QLabel(meta_card);
    m_cycle_value->setWordWrap(true);
    meta_layout->addRow(tr("Cycle"), m_cycle_value);

    meta_card_layout->addLayout(meta_layout);
    right_layout->addWidget(meta_card);

    auto *help_card = new QWidget(right_panel);
    help_card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *help_layout = new QVBoxLayout(help_card);
    help_layout->setContentsMargins(0, 0, 0, 0);
    help_layout->setSpacing(4);

    m_help_text = new QTextBrowser(help_card);
    m_help_text->setObjectName(QStringLiteral("vitaThemesHelpText"));
    m_help_text->setMinimumHeight(104);
    m_help_text->setMaximumHeight(104);
    m_help_text->setFocusPolicy(Qt::NoFocus);
    m_help_text->setFrameShape(QFrame::NoFrame);
    m_help_text->setReadOnly(true);
    m_help_text->setOpenExternalLinks(true);
    m_help_text->document()->setDocumentMargin(0);
    help_layout->addWidget(m_help_text);
    right_layout->addWidget(help_card);

    right_layout->addStretch(1);

    auto *buttons = new QWidget(this);
    auto *buttons_layout = new QHBoxLayout(buttons);
    buttons_layout->setContentsMargins(0, 0, 0, 0);
    buttons_layout->setSpacing(8);

    m_refresh_button = new QPushButton(tr("Refresh"), buttons);
    buttons_layout->addWidget(m_refresh_button);
    register_description(
        m_refresh_button,
        tr("Refresh"),
        tr("Rescan the Vita theme folder and update the installed theme list."));

    m_open_folder_button = new QPushButton(tr("Open Theme Folder"), buttons);
    buttons_layout->addWidget(m_open_folder_button);
    register_description(
        m_open_folder_button,
        tr("Open Theme Folder"),
        tr("Open the selected theme's folder in the file manager."));

    buttons_layout->addStretch(1);

    m_delete_button = new QPushButton(tr("Delete"), buttons);
    buttons_layout->addWidget(m_delete_button);
    register_description(
        m_delete_button,
        tr("Delete"),
        tr("Remove the selected installed Vita theme from ux0/theme."));

    m_apply_button = new QPushButton(tr("Apply"), buttons);
    m_apply_button->setDefault(true);
    buttons_layout->addWidget(m_apply_button);
    register_description(
        m_apply_button,
        tr("Apply"),
        tr("Generate and apply the emulator theme using the currently selected background, cycling options, and appearance settings."));

    root_layout->addWidget(buttons);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    connect(m_theme_list, &QListWidget::currentRowChanged,
        this, &VitaThemesDialog::apply_theme_selection);
    connect(m_refresh_button, &QPushButton::clicked,
        this, &VitaThemesDialog::reload);
    connect(m_apply_button, &QPushButton::clicked,
        this, &VitaThemesDialog::apply_selected_theme);
    connect(m_delete_button, &QPushButton::clicked,
        this, &VitaThemesDialog::delete_selected_theme);
    connect(m_open_folder_button, &QPushButton::clicked,
        this, &VitaThemesDialog::open_selected_theme_folder);
    connect(m_readability_slider, &QSlider::valueChanged,
        this, [this] {
            load_selection_into_ui(selection_from_ui());
        });
    connect(m_normalize_fonts_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        Q_UNUSED(checked);
        load_selection_into_ui(selection_from_ui());
    });
    connect(m_cycle_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        Q_UNUSED(checked);
        load_selection_into_ui(selection_from_ui());
    });
    connect(m_cycle_interval_spinbox, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        Q_UNUSED(value);
        load_selection_into_ui(selection_from_ui());
    });

    update_readability_label();
    sync_cycle_controls();
    reset_description();
}

void VitaThemesDialog::restore_persistent_state() {
    const QSize preferred_dialog_size(1120, 640);

    if (!m_gui_settings) {
        resize(preferred_dialog_size);
        return;
    }

    if (!restoreGeometry(m_gui_settings->get_value(gui::vt_geometry).toByteArray()))
        resize(preferred_dialog_size);

    const int capped_height = std::max(sizeHint().height(), minimumSizeHint().height());
    if (height() > capped_height)
        resize(width(), capped_height);
}

void VitaThemesDialog::save_persistent_state() const {
    if (!m_gui_settings)
        return;

    m_gui_settings->set_value(gui::vt_geometry, saveGeometry(), false);
    m_gui_settings->sync();
}

void VitaThemesDialog::repopulate_theme_list(const QString &preferred_theme_id) {
    m_themes = m_theme_manager.installed_vita_themes();

    m_theme_list->blockSignals(true);
    m_theme_list->clear();

    int selected_row = -1;
    for (int index = 0; index < static_cast<int>(m_themes.size()); ++index) {
        const auto &theme = m_themes[index];
        auto *item = new QListWidgetItem(
            theme.provider.empty()
                ? to_qstring(theme.title)
                : tr("%1\n%2").arg(to_qstring(theme.title), to_qstring(theme.provider)),
            m_theme_list);
        item->setData(Qt::UserRole, to_qstring(theme.theme_id));

        if (!preferred_theme_id.isEmpty() && preferred_theme_id == to_qstring(theme.theme_id))
            selected_row = index;
    }

    m_theme_list->blockSignals(false);

    if (selected_row < 0 && !m_themes.empty())
        selected_row = 0;

    if (selected_row >= 0)
        m_theme_list->setCurrentRow(selected_row);
    else
        apply_theme_selection(-1);
}

void VitaThemesDialog::apply_theme_selection(const int row) {
    if (row < 0 || row >= static_cast<int>(m_themes.size())) {
        clear_selection_state();
        load_theme_into_ui(nullptr);
        sync_preview();
        sync_metadata();
        sync_cycle_controls();
        sync_actions();
        return;
    }

    const auto &theme = m_themes[static_cast<std::size_t>(row)];
    gui::VitaThemeSelection selection;
    if (const auto applied_selection = m_theme_manager.applied_vita_theme_selection();
        applied_selection && applied_selection->theme_id == to_qstring(theme.theme_id)) {
        selection = *applied_selection;
    } else {
        selection.theme_id = to_qstring(theme.theme_id);
    }

    load_selection_into_ui(std::move(selection), std::nullopt, true, true);
    reset_description();
}

void VitaThemesDialog::clear_selection_state() {
    m_selected_background_id.clear();
    m_preview_background_id.clear();
    m_loaded_preview_background_id.clear();
    m_selected_cycle_background_ids.clear();
    m_preview_pixmap = {};
}

gui::VitaThemeSelection VitaThemesDialog::selection_from_ui() const {
    gui::VitaThemeSelection selection;
    if (const auto *theme = current_theme())
        selection.theme_id = to_qstring(theme->theme_id);

    selection.background_id = m_selected_background_id;
    selection.readability = m_readability_slider ? m_readability_slider->value() : 55;
    selection.normalize_font_colors = m_normalize_fonts_checkbox
        ? m_normalize_fonts_checkbox->isChecked()
        : true;
    selection.cycle_enabled = m_cycle_checkbox && m_cycle_checkbox->isChecked();
    selection.cycle_interval_seconds = m_cycle_interval_spinbox
        ? m_cycle_interval_spinbox->value()
        : 15;
    selection.cycle_background_ids = m_selected_cycle_background_ids;
    return selection;
}

void VitaThemesDialog::load_selection_into_ui(
    gui::VitaThemeSelection selection,
    const std::optional<QString> preview_background_id,
    const bool follow_primary_preview,
    const bool reload_theme_ui) {
    const auto *theme = current_theme();
    if (!theme) {
        clear_selection_state();
        load_theme_into_ui(nullptr);
        sync_preview();
        sync_metadata();
        sync_cycle_controls();
        sync_actions();
        return;
    }

    selection.theme_id = to_qstring(theme->theme_id);
    const gui::VitaThemeSelection normalized_selection = m_theme_manager.normalize_selection_for_theme(*theme, std::move(selection));

    {
        const QSignalBlocker readability_blocker(m_readability_slider);
        const QSignalBlocker normalize_fonts_blocker(m_normalize_fonts_checkbox);
        const QSignalBlocker cycle_checkbox_blocker(m_cycle_checkbox);
        const QSignalBlocker cycle_interval_blocker(m_cycle_interval_spinbox);
        m_readability_slider->setValue(normalized_selection.readability);
        m_normalize_fonts_checkbox->setChecked(normalized_selection.normalize_font_colors);
        m_cycle_checkbox->setChecked(normalized_selection.cycle_enabled);
        m_cycle_interval_spinbox->setValue(normalized_selection.cycle_interval_seconds);
    }

    m_selected_background_id = normalized_selection.background_id;
    m_selected_cycle_background_ids = normalized_selection.cycle_background_ids;
    m_preview_background_id = follow_primary_preview
        ? m_selected_background_id
        : resolve_preview_background_id(
              *theme,
              preview_background_id ? *preview_background_id : m_preview_background_id);

    if (reload_theme_ui || m_loaded_theme_id != normalized_selection.theme_id)
        load_theme_into_ui(theme);

    update_readability_label();
    sync_background_rows();
    sync_preview();
    sync_metadata();
    sync_cycle_controls();
    sync_actions();
}

void VitaThemesDialog::load_theme_into_ui(const gui::VitaThemeInfo *theme) {
    if (!m_background_list)
        return;

    const QSignalBlocker list_blocker(m_background_list);
    m_background_rows.clear();
    m_background_list->clear();
    if (m_primary_background_group) {
        delete m_primary_background_group;
        m_primary_background_group = nullptr;
    }

    m_loaded_theme_id = theme ? to_qstring(theme->theme_id) : QString();
    m_loaded_preview_background_id.clear();
    if (!theme)
        return;

    m_primary_background_group = new QButtonGroup(this);
    m_primary_background_group->setExclusive(true);

    for (const auto &background : theme->background_options) {
        BackgroundRow row;
        row.id = to_qstring(background.id);

        row.item = new QTreeWidgetItem(m_background_list);
        row.item->setData(0, Qt::UserRole, row.id);
        row.item->setText(0, to_qstring(background.label));
        row.item->setSizeHint(0, QSize(0, 34));

        auto *primary_widget = new QWidget(m_background_list);
        primary_widget->setProperty("themeRole", QStringLiteral("backgroundCell"));
        auto *primary_layout = new QHBoxLayout(primary_widget);
        primary_layout->setContentsMargins(6, 0, 6, 0);
        primary_layout->setSpacing(0);
        primary_layout->setAlignment(Qt::AlignCenter);

        row.primary_radio = new QRadioButton(primary_widget);
        row.primary_radio->setProperty("themeRole", QStringLiteral("backgroundCellControl"));
        m_primary_background_group->addButton(row.primary_radio);
        connect(row.primary_radio, &QRadioButton::clicked, this, [this, id = row.id] {
            set_selected_background(id);
        });
        primary_layout->addWidget(row.primary_radio);
        m_background_list->setItemWidget(row.item, 1, primary_widget);

        auto *cycle_widget = new QWidget(m_background_list);
        cycle_widget->setProperty("themeRole", QStringLiteral("backgroundCell"));
        auto *cycle_layout = new QHBoxLayout(cycle_widget);
        cycle_layout->setContentsMargins(6, 0, 6, 0);
        cycle_layout->setSpacing(0);
        cycle_layout->setAlignment(Qt::AlignCenter);

        row.cycle_checkbox = new QCheckBox(cycle_widget);
        row.cycle_checkbox->setProperty("themeRole", QStringLiteral("backgroundCellControl"));
        connect(row.cycle_checkbox, &QCheckBox::toggled, this, [this, id = row.id](const bool checked) {
            QStringList next_ids = m_selected_cycle_background_ids;
            if (checked) {
                if (!next_ids.contains(id))
                    next_ids.append(id);
            } else {
                next_ids.removeAll(id);
            }
            set_selected_cycle_backgrounds(next_ids);
        });
        cycle_layout->addWidget(row.cycle_checkbox);
        m_background_list->setItemWidget(row.item, 2, cycle_widget);

        const QString description = tr("Set this as the primary background for the generated theme. %1 backgrounds can also be included in cycling.")
                                        .arg(background_source_label(background));
        register_description(primary_widget, tr("Primary Background"), description);
        register_description(row.primary_radio, tr("Primary Background"), description);
        register_description(
            cycle_widget,
            tr("Include In Cycle"),
            tr("Include this background when cycling is enabled. The primary background is always kept in the cycle order."));
        register_description(
            row.cycle_checkbox,
            tr("Include In Cycle"),
            tr("Include this background when cycling is enabled. The primary background is always kept in the cycle order."));

        m_background_rows.push_back(row);
    }
}

void VitaThemesDialog::sync_background_rows() {
    if (!m_background_list)
        return;

    const auto *theme = current_theme();
    const bool can_cycle = theme && theme->background_options.size() > 1;
    const QString current_preview_id = theme
        ? resolve_preview_background_id(*theme, m_preview_background_id)
        : QString();
    QTreeWidgetItem *current_item = nullptr;

    const QSignalBlocker list_blocker(m_background_list);
    for (auto &row : m_background_rows) {
        if (row.primary_radio) {
            const QSignalBlocker blocker(row.primary_radio);
            row.primary_radio->setChecked(row.id == m_selected_background_id);
        }

        if (row.cycle_checkbox) {
            const QSignalBlocker blocker(row.cycle_checkbox);
            row.cycle_checkbox->setChecked(m_selected_cycle_background_ids.contains(row.id));
            row.cycle_checkbox->setEnabled(can_cycle && row.id != m_selected_background_id);
        }

        if (row.item && row.id == current_preview_id)
            current_item = row.item;
    }

    if (!current_item) {
        for (const auto &row : m_background_rows) {
            if (row.item && row.id == m_selected_background_id) {
                current_item = row.item;
                break;
            }
        }
    }

    if (!current_item && !m_background_rows.empty())
        current_item = m_background_rows.front().item;

    if (m_background_list->currentItem() != current_item)
        m_background_list->setCurrentItem(current_item);
}

void VitaThemesDialog::set_selected_background(const QString &background_id) {
    auto selection = selection_from_ui();
    selection.background_id = background_id;
    load_selection_into_ui(std::move(selection), std::nullopt, true);
}

void VitaThemesDialog::set_preview_background(const QString &background_id) {
    load_selection_into_ui(selection_from_ui(), background_id);
}

void VitaThemesDialog::set_selected_cycle_backgrounds(const QStringList &background_ids) {
    auto selection = selection_from_ui();
    selection.cycle_background_ids = background_ids;
    load_selection_into_ui(std::move(selection));
}

void VitaThemesDialog::sync_preview() {
    auto *preview_label = dynamic_cast<AspectRatioPreviewLabel *>(m_preview);
    const auto *background = current_preview_background();
    if (!background) {
        m_loaded_preview_background_id.clear();
        if (preview_label)
            preview_label->set_aspect_size(k_default_preview_aspect);
        m_preview->setText(tr("No preview available"));
        m_preview->setPixmap(QPixmap());
        m_preview_pixmap = {};
        return;
    }

    const QString background_id = to_qstring(background->id);
    if (m_loaded_preview_background_id == background_id && !m_preview_pixmap.isNull()) {
        update_preview_pixmap();
        return;
    }

    const fs::path preview_path = background->preview_path.empty()
        ? background->asset_path
        : background->preview_path;
    const QString qt_path = gui::utils::to_qt_path(preview_path);
    QPixmap pixmap(qt_path);
    if (pixmap.isNull()) {
        m_loaded_preview_background_id.clear();
        if (preview_label)
            preview_label->set_aspect_size(k_default_preview_aspect);
        m_preview->setText(tr("Unable to load preview"));
        m_preview->setPixmap(QPixmap());
        m_preview_pixmap = {};
        return;
    }

    m_loaded_preview_background_id = background_id;
    m_preview->setText(QString());
    if (preview_label)
        preview_label->set_aspect_size(pixmap.size());
    m_preview_pixmap = pixmap;
    update_preview_pixmap();
}

void VitaThemesDialog::sync_metadata() {
    const auto *theme = current_theme();
    if (!theme) {
        m_theme_title->setText(tr("No themes installed"));
        m_theme_provider->setText(tr("Install or copy a Vita theme into ux0/theme to get started."));
        m_version_value->clear();
        m_theme_id_value->clear();
        m_size_value->clear();
        m_updated_value->clear();
        m_background_value->clear();
        m_cycle_value->clear();
        if (m_cycle_summary_value)
            m_cycle_summary_value->clear();
        return;
    }

    m_theme_title->setText(to_qstring(theme->title));
    m_theme_provider->setText(theme->provider.empty() ? tr("Unknown provider") : to_qstring(theme->provider));
    m_version_value->setText(theme->content_version.empty() ? tr("Unknown") : to_qstring(theme->content_version));
    m_theme_id_value->setText(to_qstring(theme->theme_id));
    m_size_value->setText(QLocale().formattedDataSize(static_cast<qint64>(theme->installed_size)));
    m_updated_value->setText(theme->updated_time == 0
            ? tr("Unknown")
            : QLocale().toString(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(theme->updated_time)), QLocale::LongFormat));

    if (const auto *background = current_background()) {
        m_background_value->setText(tr("%1: %2")
                                        .arg(background_source_label(*background), to_qstring(background->label)));
    } else {
        m_background_value->clear();
    }

    if (m_cycle_checkbox && m_cycle_checkbox->isChecked()) {
        m_cycle_value->setText(tr("%1 backgrounds every %2 seconds")
                                   .arg(m_selected_cycle_background_ids.size())
                                   .arg(m_cycle_interval_spinbox ? m_cycle_interval_spinbox->value() : 15));
    } else if (m_selected_cycle_background_ids.size() > 1) {
        m_cycle_value->setText(tr("Off (%1 saved)").arg(m_selected_cycle_background_ids.size()));
    } else {
        m_cycle_value->setText(tr("Off"));
    }

    update_package_thumbnail();
}

void VitaThemesDialog::sync_actions() {
    const bool has_theme = current_theme() != nullptr;
    const bool has_background = current_background() != nullptr;
    const bool has_cycle_selection = !m_selected_cycle_background_ids.isEmpty();
    m_apply_button->setEnabled(has_theme && has_background && has_cycle_selection);
    m_delete_button->setEnabled(has_theme);
    m_open_folder_button->setEnabled(has_theme);
}

void VitaThemesDialog::sync_cycle_controls() {
    const auto *theme = current_theme();
    const int background_count = theme ? static_cast<int>(theme->background_options.size()) : 0;
    const bool can_cycle = background_count > 1;
    bool cycle_state_changed = false;

    if (!can_cycle && m_cycle_checkbox && m_cycle_checkbox->isChecked()) {
        const QSignalBlocker blocker(m_cycle_checkbox);
        m_cycle_checkbox->setChecked(false);
        cycle_state_changed = true;
    }

    if (m_cycle_checkbox)
        m_cycle_checkbox->setEnabled(can_cycle);
    if (m_cycle_interval_spinbox)
        m_cycle_interval_spinbox->setEnabled(can_cycle && m_cycle_checkbox && m_cycle_checkbox->isChecked());
    if (m_background_list)
        m_background_list->setEnabled(background_count > 0);
    if (m_cycle_summary_value) {
        if (!theme) {
            m_cycle_summary_value->clear();
        } else if (!can_cycle) {
            m_cycle_summary_value->setText(tr("Single background"));
        } else if (m_cycle_checkbox && m_cycle_checkbox->isChecked()) {
            m_cycle_summary_value->setText(tr("%1 selected").arg(m_selected_cycle_background_ids.size()));
        } else if (m_selected_cycle_background_ids.size() > 1) {
            m_cycle_summary_value->setText(tr("%1 saved").arg(m_selected_cycle_background_ids.size()));
        } else {
            m_cycle_summary_value->setText(tr("Off"));
        }
    }

    if (cycle_state_changed)
        sync_metadata();
}

void VitaThemesDialog::update_preview_pixmap() {
    if (m_preview_pixmap.isNull()) {
        m_preview->setPixmap(QPixmap());
        return;
    }

    const QSize target_size = m_preview->size() - QSize(12, 12);
    if (target_size.width() <= 0 || target_size.height() <= 0)
        return;

    m_preview->setPixmap(m_preview_pixmap.scaled(
        target_size,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation));
}

void VitaThemesDialog::update_package_thumbnail() {
    const auto *theme = current_theme();
    if (!theme || theme->package_thumbnail_path.empty() || !fs::exists(theme->package_thumbnail_path)) {
        m_package_thumbnail->setPixmap(QPixmap());
        m_package_thumbnail->setText(tr("No package thumbnail"));
        return;
    }

    const QPixmap pixmap(gui::utils::to_qt_path(theme->package_thumbnail_path));
    if (pixmap.isNull()) {
        m_package_thumbnail->setPixmap(QPixmap());
        m_package_thumbnail->setText(tr("No package thumbnail"));
        return;
    }

    m_package_thumbnail->setText(QString());
    m_package_thumbnail->setPixmap(pixmap.scaled(
        m_package_thumbnail->size() - QSize(12, 12),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation));
}

void VitaThemesDialog::update_readability_label() {
    if (!m_readability_slider || !m_readability_value)
        return;

    m_readability_value->setText(tr("%1%").arg(m_readability_slider->value()));
}

void VitaThemesDialog::apply_selected_theme() {
    const auto *theme = current_theme();
    const auto *background = current_background();
    if (!theme || !background)
        return;

    const gui::VitaThemeSelection selection = selection_from_ui();

    if (!m_theme_manager.apply_vita_theme_selection(selection, true)) {
        QMessageBox::warning(
            this,
            tr("Apply Failed"),
            tr("Vita3K could not generate or apply the selected theme stylesheet."));
        return;
    }
}

void VitaThemesDialog::delete_selected_theme() {
    const auto *theme = current_theme();
    if (!theme)
        return;

    const auto result = QMessageBox::question(
        this,
        tr("Delete Theme"),
        tr("Delete \"%1\" from ux0/theme?").arg(to_qstring(theme->title)),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (result != QMessageBox::Yes)
        return;

    const QString deleting_theme_id = to_qstring(theme->theme_id);
    const auto applied_selection = m_theme_manager.applied_vita_theme_selection();
    const bool was_active = applied_selection && applied_selection->theme_id == deleting_theme_id;
    boost::system::error_code error_code;
    fs::remove_all(theme->installed_path, error_code);
    if (error_code) {
        QMessageBox::warning(
            this,
            tr("Delete Failed"),
            tr("Vita3K could not remove the selected theme folder."));
        return;
    }

    if (was_active && m_gui_settings) {
        m_theme_manager.clear_applied_vita_theme();
        m_gui_settings->set_value(gui::m_currentStylesheet, gui::DarkStylesheet, false);
        m_gui_settings->sync();
        m_theme_manager.apply_theme(gui::DarkStylesheet, true);
    }
    m_theme_manager.refresh_vita_theme_catalog();
    repopulate_theme_list();
}

void VitaThemesDialog::open_selected_theme_folder() const {
    const auto *theme = current_theme();
    if (!theme)
        return;

    gui::utils::open_dir(theme->installed_path);
}

void VitaThemesDialog::register_description(QWidget *widget, const QString &title, const QString &description) {
    if (!widget)
        return;

    widget->setProperty("_desc_title", title);
    widget->setProperty("_desc", description);
    widget->installEventFilter(this);
}

void VitaThemesDialog::set_description(const QString &title, const QString &text) {
    if (!m_help_text)
        return;

    m_help_text->setHtml(gui::utils::format_help_html(title, text));
}

void VitaThemesDialog::reset_description() {
    set_description(tr("Vita Themes"), default_description());
}

QString VitaThemesDialog::default_description() const {
    if (!current_theme()) {
        return tr("Select an installed Vita theme to preview its backgrounds and generate a matching emulator theme.");
    }

    return tr("Choose one primary background, decide which backgrounds should appear in the cycle, adjust readability options, then apply the generated theme.");
}

QString VitaThemesDialog::resolve_preview_background_id(
    const gui::VitaThemeInfo &theme,
    const QString &background_id) const {
    if (theme.background_options.empty())
        return {};

    if (!background_id.isEmpty()) {
        const auto matches = [&background_id](const gui::VitaThemeBackgroundOption &option) {
            return background_id == to_qstring(option.id);
        };
        if (std::find_if(theme.background_options.begin(), theme.background_options.end(), matches) != theme.background_options.end())
            return background_id;
    }

    return m_selected_background_id.isEmpty()
        ? to_qstring(theme.background_options.front().id)
        : m_selected_background_id;
}

const gui::VitaThemeInfo *VitaThemesDialog::current_theme() const {
    const int row = m_theme_list ? m_theme_list->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(m_themes.size()))
        return nullptr;

    return &m_themes[static_cast<std::size_t>(row)];
}

const gui::VitaThemeBackgroundOption *VitaThemesDialog::current_background() const {
    const auto *theme = current_theme();
    if (!theme)
        return nullptr;

    for (const auto &background : theme->background_options) {
        if (m_selected_background_id == to_qstring(background.id))
            return &background;
    }

    return theme->background_options.empty() ? nullptr : &theme->background_options.front();
}

const gui::VitaThemeBackgroundOption *VitaThemesDialog::current_preview_background() const {
    const auto *theme = current_theme();
    if (!theme)
        return nullptr;

    for (const auto &background : theme->background_options) {
        if (m_preview_background_id == to_qstring(background.id))
            return &background;
    }

    return current_background();
}
