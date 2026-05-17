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

#include <gui-qt/debug_widget.h>

#include <cpu/functions.h>
#include <emuenv/state.h>
#include <kernel/state.h>
#include <kernel/thread/thread_state.h>
#include <mem/functions.h>
#include <mem/state.h>
#include <mem/util.h>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <fmt/format.h>

DebugWidget::DebugWidget(EmuEnvState &emuenv, QWidget *parent)
    : custom_dock_widget(tr("Debug"), parent)
    , emuenv(emuenv) {
    setup_ui();

    m_refresh_timer = new QTimer(this);
    m_refresh_timer->setInterval(500);
    connect(m_refresh_timer, &QTimer::timeout, this, &DebugWidget::refresh_current_tab);
    m_refresh_timer->start();
}

void DebugWidget::show_tab(int index) {
    if (index >= 0 && index < Tab::TabCount)
        m_tabs->setCurrentIndex(index);

    show();
    raise();
    activateWindow();
    refresh_current_tab();
}

void DebugWidget::setup_ui() {
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    m_tabs = new QTabWidget(container);

    m_threads_tree = create_tree({ tr("ID"), tr("Name"), tr("Status"), tr("Stack") });
    m_threads_tree->setColumnWidth(0, 100);
    m_threads_tree->setColumnWidth(1, 250);
    m_threads_tree->setColumnWidth(2, 100);
    connect(m_threads_tree, &QTreeWidget::itemDoubleClicked,
        this, &DebugWidget::on_thread_double_clicked);
    m_tabs->addTab(m_threads_tree, tr("Threads"));

    m_mutexes_tree = create_tree({ tr("ID"), tr("Name"), tr("Lock Count"), tr("Attributes"), tr("Waiting Threads"), tr("Owner") });
    m_mutexes_tree->setColumnWidth(0, 100);
    m_mutexes_tree->setColumnWidth(1, 220);
    m_tabs->addTab(m_mutexes_tree, tr("Mutexes"));

    m_lw_mutexes_tree = create_tree({ tr("ID"), tr("Name"), tr("Lock Count"), tr("Attributes"), tr("Waiting Threads"), tr("Owner") });
    m_lw_mutexes_tree->setColumnWidth(0, 100);
    m_lw_mutexes_tree->setColumnWidth(1, 220);
    m_tabs->addTab(m_lw_mutexes_tree, tr("LW Mutexes"));

    m_condvars_tree = create_tree({ tr("ID"), tr("Name"), tr("Attributes"), tr("Waiting Threads") });
    m_condvars_tree->setColumnWidth(0, 100);
    m_condvars_tree->setColumnWidth(1, 250);
    m_tabs->addTab(m_condvars_tree, tr("Condvars"));

    m_lw_condvars_tree = create_tree({ tr("ID"), tr("Name"), tr("Attributes"), tr("Waiting Threads") });
    m_lw_condvars_tree->setColumnWidth(0, 100);
    m_lw_condvars_tree->setColumnWidth(1, 250);
    m_tabs->addTab(m_lw_condvars_tree, tr("LW Condvars"));

    m_semaphores_tree = create_tree({ tr("ID"), tr("Name"), tr("Value"), tr("Max"), tr("Waiting Threads") });
    m_semaphores_tree->setColumnWidth(0, 100);
    m_semaphores_tree->setColumnWidth(1, 250);
    m_tabs->addTab(m_semaphores_tree, tr("Semaphores"));

    m_event_flags_tree = create_tree({ tr("ID"), tr("Name"), tr("Flags"), tr("Attributes"), tr("Waiting Threads") });
    m_event_flags_tree->setColumnWidth(0, 100);
    m_event_flags_tree->setColumnWidth(1, 250);
    m_tabs->addTab(m_event_flags_tree, tr("Event Flags"));

    m_allocations_tree = create_tree({ tr("Page"), tr("Name"), tr("Address Range"), tr("Size (KiB)"), tr("Pages") });
    m_allocations_tree->setColumnWidth(0, 80);
    m_allocations_tree->setColumnWidth(1, 280);
    m_allocations_tree->setColumnWidth(2, 200);
    m_tabs->addTab(m_allocations_tree, tr("Allocations"));

    m_disasm_tab = new QWidget(m_tabs);
    auto *disasm_layout = new QVBoxLayout(m_disasm_tab);
    disasm_layout->setContentsMargins(4, 4, 4, 4);

    auto *disasm_controls = new QHBoxLayout();
    disasm_controls->addWidget(new QLabel(tr("Address"), m_disasm_tab));
    m_disasm_address = new QLineEdit(m_disasm_tab);
    m_disasm_address->setMaxLength(8);
    m_disasm_address->setPlaceholderText(QStringLiteral("00000000"));
    m_disasm_address->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_disasm_address->setMaximumWidth(100);
    disasm_controls->addWidget(m_disasm_address);

    disasm_controls->addWidget(new QLabel(tr("Count"), m_disasm_tab));
    m_disasm_count = new QSpinBox(m_disasm_tab);
    m_disasm_count->setRange(1, 10000);
    m_disasm_count->setValue(100);
    m_disasm_count->setMaximumWidth(80);
    disasm_controls->addWidget(m_disasm_count);

    disasm_controls->addWidget(new QLabel(tr("Arch"), m_disasm_tab));
    m_disasm_arch = new QComboBox(m_disasm_tab);
    m_disasm_arch->addItems({ QStringLiteral("ARM"), QStringLiteral("THUMB") });
    disasm_controls->addWidget(m_disasm_arch);

    disasm_controls->addStretch();
    disasm_layout->addLayout(disasm_controls);

    m_disasm_output = new QPlainTextEdit(m_disasm_tab);
    m_disasm_output->setReadOnly(true);
    m_disasm_output->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_disasm_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    disasm_layout->addWidget(m_disasm_output);

    connect(m_disasm_address, &QLineEdit::returnPressed, this, &DebugWidget::on_disassembly_evaluate);
    connect(m_disasm_count, &QSpinBox::editingFinished, this, &DebugWidget::on_disassembly_evaluate);
    connect(m_disasm_arch, &QComboBox::currentIndexChanged, this, [this](int) { on_disassembly_evaluate(); });

    m_tabs->addTab(m_disasm_tab, tr("Disassembly"));

    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int) {
        refresh_current_tab();
    });

    layout->addWidget(m_tabs);
    setWidget(container);

    setMinimumSize(700, 400);
    resize(850, 500);
}

QTreeWidget *DebugWidget::create_tree(const QStringList &headers) {
    auto *tree = new QTreeWidget(this);
    tree->setHeaderLabels(headers);
    tree->setRootIsDecorated(false);
    tree->setSortingEnabled(true);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setUniformRowHeights(true);
    tree->header()->setStretchLastSection(true);
    tree->setFont(QFont(QStringLiteral("Consolas"), 9));
    return tree;
}

void DebugWidget::refresh_current_tab() {
    if (!isVisible())
        return;

    switch (m_tabs->currentIndex()) {
    case Tab::Threads:
        refresh_threads();
        break;
    case Tab::Mutexes:
        refresh_mutexes();
        break;
    case Tab::LwMutexes:
        refresh_lw_mutexes();
        break;
    case Tab::Condvars:
        refresh_condvars();
        break;
    case Tab::LwCondvars:
        refresh_lw_condvars();
        break;
    case Tab::Semaphores:
        refresh_semaphores();
        break;
    case Tab::EventFlags:
        refresh_event_flags();
        break;
    case Tab::Allocations:
        refresh_allocations();
        break;
    case Tab::Disassembly:
        break;
    }
}

static QString thread_status_string(ThreadStatus status) {
    switch (status) {
    case ThreadStatus::run:
        return QStringLiteral("Running");
    case ThreadStatus::wait:
        return QStringLiteral("Waiting");
    case ThreadStatus::dormant:
        return QStringLiteral("Dormant");
    case ThreadStatus::suspend:
        return QStringLiteral("Suspended");
    default:
        return QStringLiteral("Unknown");
    }
}

void DebugWidget::refresh_threads() {
    m_threads_tree->clear();

    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    for (const auto &[id, th] : emuenv.kernel.threads) {
        auto *item = new QTreeWidgetItem(m_threads_tree);
        item->setData(0, Qt::UserRole, static_cast<uint>(id));
        item->setText(0, QStringLiteral("0x%1").arg(id, 8, 16, QLatin1Char('0')).toUpper());
        item->setText(1, QString::fromStdString(th->name));
        item->setText(2, thread_status_string(th->status));
        item->setText(3, QStringLiteral("0x%1").arg(th->stack.get(), 8, 16, QLatin1Char('0')).toUpper());
    }
}

void DebugWidget::refresh_mutexes() {
    m_mutexes_tree->clear();

    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    for (const auto &[id, mutex] : emuenv.kernel.mutexes) {
        auto *item = new QTreeWidgetItem(m_mutexes_tree);
        item->setText(0, QStringLiteral("0x%1").arg(id, 8, 16, QLatin1Char('0')).toUpper());
        item->setText(1, QString::fromUtf8(mutex->name));
        item->setText(2, QString::number(mutex->lock_count));
        item->setText(3, QString::number(mutex->attr));
        item->setText(4, QString::number(mutex->waiting_threads ? mutex->waiting_threads->size() : 0));
        item->setText(5, mutex->owner ? QString::fromStdString(mutex->owner->name) : tr("not owned"));
    }
}

void DebugWidget::refresh_lw_mutexes() {
    m_lw_mutexes_tree->clear();

    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    for (const auto &[id, mutex] : emuenv.kernel.lwmutexes) {
        auto *item = new QTreeWidgetItem(m_lw_mutexes_tree);
        item->setText(0, QStringLiteral("0x%1").arg(id, 8, 16, QLatin1Char('0')).toUpper());
        item->setText(1, QString::fromUtf8(mutex->name));
        item->setText(2, QString::number(mutex->lock_count));
        item->setText(3, QString::number(mutex->attr));
        item->setText(4, QString::number(mutex->waiting_threads ? mutex->waiting_threads->size() : 0));
        item->setText(5, mutex->owner ? QString::fromStdString(mutex->owner->name) : tr("not owned"));
    }
}

void DebugWidget::refresh_condvars() {
    m_condvars_tree->clear();

    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    for (const auto &[id, cv] : emuenv.kernel.condvars) {
        auto *item = new QTreeWidgetItem(m_condvars_tree);
        item->setText(0, QStringLiteral("0x%1").arg(id, 8, 16, QLatin1Char('0')).toUpper());
        item->setText(1, QString::fromUtf8(cv->name));
        item->setText(2, QString::number(cv->attr));
        item->setText(3, QString::number(cv->waiting_threads ? cv->waiting_threads->size() : 0));
    }
}

void DebugWidget::refresh_lw_condvars() {
    m_lw_condvars_tree->clear();

    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    for (const auto &[id, cv] : emuenv.kernel.lwcondvars) {
        auto *item = new QTreeWidgetItem(m_lw_condvars_tree);
        item->setText(0, QStringLiteral("0x%1").arg(id, 8, 16, QLatin1Char('0')).toUpper());
        item->setText(1, QString::fromUtf8(cv->name));
        item->setText(2, QString::number(cv->attr));
        item->setText(3, QString::number(cv->waiting_threads ? cv->waiting_threads->size() : 0));
    }
}

void DebugWidget::refresh_semaphores() {
    m_semaphores_tree->clear();

    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    for (const auto &[id, sema] : emuenv.kernel.semaphores) {
        auto *item = new QTreeWidgetItem(m_semaphores_tree);
        item->setText(0, QStringLiteral("0x%1").arg(id, 8, 16, QLatin1Char('0')).toUpper());
        item->setText(1, QString::fromUtf8(sema->name));
        item->setText(2, QStringLiteral("%1 / %2").arg(sema->val).arg(sema->max));
        item->setText(3, QString::number(sema->max));
        item->setText(4, QString::number(sema->waiting_threads ? sema->waiting_threads->size() : 0));
    }
}

void DebugWidget::refresh_event_flags() {
    m_event_flags_tree->clear();

    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    for (const auto &[id, ef] : emuenv.kernel.eventflags) {
        auto *item = new QTreeWidgetItem(m_event_flags_tree);
        item->setText(0, QStringLiteral("0x%1").arg(id, 8, 16, QLatin1Char('0')).toUpper());
        item->setText(1, QString::fromUtf8(ef->name));
        item->setText(2, QStringLiteral("0x%1").arg(static_cast<uint>(ef->flags), 8, 16, QLatin1Char('0')).toUpper());
        item->setText(3, QString::number(ef->attr));
        item->setText(4, QString::number(ef->waiting_threads ? ef->waiting_threads->size() : 0));
    }
}

static const QStringList alloc_blacklist = {
    QStringLiteral("NULL"),
    QStringLiteral("export_sceGxmDisplayQueueAddEntry")
};

void DebugWidget::refresh_allocations() {
    m_allocations_tree->clear();

    const std::lock_guard<std::mutex> lock(emuenv.mem.generation_mutex);

    for (const auto &[page_num, page_name] : emuenv.mem.page_name_map) {
        if (alloc_blacklist.contains(QString::fromStdString(page_name)))
            continue;

        const auto &page = emuenv.mem.alloc_table[page_num];

        auto *item = new QTreeWidgetItem(m_allocations_tree);
        item->setText(0, QString::number(page_num));
        item->setText(1, QString::fromStdString(page_name));
        item->setText(2, QStringLiteral("0x%1 - 0x%2").arg(static_cast<uint>(page_num * KiB(4)), 8, 16, QLatin1Char('0')).arg(static_cast<uint>((page_num + page.size) * KiB(4)), 8, 16, QLatin1Char('0')).toUpper());
        item->setText(3, QString::number(page.size * 4));
        item->setText(4, QString::number(page.size));
    }
}

void DebugWidget::on_thread_double_clicked(QTreeWidgetItem *item, int /*column*/) {
    if (!item)
        return;

    const SceUID thread_id = static_cast<SceUID>(item->data(0, Qt::UserRole).toUInt());
    ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    if (!thread) {
        QMessageBox::warning(this, tr("Thread Not Found"),
            tr("Thread 0x%1 no longer exists.").arg(thread_id, 8, 16, QLatin1Char('0')));
        return;
    }

    CPUState &cpu = *thread->cpu;

    const uint32_t pc = read_pc(cpu);
    const uint32_t sp = read_sp(cpu);
    const uint32_t lr = read_lr(cpu);

    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Thread: %1 (0x%2)")
                            .arg(QString::fromStdString(thread->name))
                            .arg(thread_id, 8, 16, QLatin1Char('0')));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setMinimumSize(420, 480);

    auto *layout = new QVBoxLayout(dlg);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    auto make_hex = [](uint32_t val) {
        return QStringLiteral("0x%1").arg(val, 8, 16, QLatin1Char('0')).toUpper();
    };

    form->addRow(tr("Name:"), new QLabel(QString::fromStdString(thread->name), dlg));
    form->addRow(tr("Status:"), new QLabel(thread_status_string(thread->status), dlg));
    form->addRow(tr("PC:"), new QLabel(make_hex(pc), dlg));
    form->addRow(tr("SP:"), new QLabel(make_hex(sp), dlg));
    form->addRow(tr("LR:"), new QLabel(make_hex(lr), dlg));
    form->addRow(tr("Executing:"), new QLabel(QString::fromStdString(disassemble(cpu, pc)), dlg));

    layout->addLayout(form);

    auto *reg_label = new QLabel(tr("Registers"), dlg);
    reg_label->setStyleSheet(QStringLiteral("font-weight: bold; margin-top: 8px;"));
    layout->addWidget(reg_label);

    auto *reg_tree = new QTreeWidget(dlg);
    reg_tree->setHeaderLabels({ tr("Register"), tr("Value") });
    reg_tree->setRootIsDecorated(false);
    reg_tree->setFont(QFont(QStringLiteral("Consolas"), 9));
    reg_tree->header()->setStretchLastSection(true);

    for (int i = 0; i < 13; i++) {
        auto *row = new QTreeWidgetItem(reg_tree);
        row->setText(0, QStringLiteral("r%1").arg(i));
        row->setText(1, make_hex(read_reg(cpu, i)));
    }
    {
        auto *row = new QTreeWidgetItem(reg_tree);
        row->setText(0, QStringLiteral("SP"));
        row->setText(1, make_hex(sp));
    }
    {
        auto *row = new QTreeWidgetItem(reg_tree);
        row->setText(0, QStringLiteral("LR"));
        row->setText(1, make_hex(lr));
    }
    {
        auto *row = new QTreeWidgetItem(reg_tree);
        row->setText(0, QStringLiteral("PC"));
        row->setText(1, make_hex(pc));
    }

    layout->addWidget(reg_tree);

    auto *stack_label = new QLabel(tr("Stack"), dlg);
    stack_label->setStyleSheet(QStringLiteral("font-weight: bold; margin-top: 8px;"));
    layout->addWidget(stack_label);

    auto *stack_tree = new QTreeWidget(dlg);
    stack_tree->setHeaderLabels({ tr("Offset"), tr("Address"), tr("Value") });
    stack_tree->setRootIsDecorated(false);
    stack_tree->setFont(QFont(QStringLiteral("Consolas"), 9));
    stack_tree->header()->setStretchLastSection(true);

    for (int i = 0; i < 15; i++) {
        const Address addr = sp + i * 4;
        auto *ptr = Ptr<uint32_t>(addr).get(emuenv.mem);
        auto *row = new QTreeWidgetItem(stack_tree);
        row->setText(0, QStringLiteral("+%1").arg(i * 4));
        row->setText(1, make_hex(addr));
        row->setText(2, ptr ? make_hex(*ptr) : tr("(invalid)"));
    }

    layout->addWidget(stack_tree);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    layout->addWidget(buttons);

    dlg->show();
}

void DebugWidget::on_disassembly_evaluate() {
    m_disasm_output->clear();

    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    if (emuenv.kernel.threads.empty()) {
        m_disasm_output->setPlainText(tr("Nothing to disassemble."));
        return;
    }

    bool ok = false;
    const uint32_t address = m_disasm_address->text().toUInt(&ok, 16);
    if (!ok || address == 0) {
        m_disasm_output->setPlainText(tr("Invalid address."));
        return;
    }

    const uint32_t count = static_cast<uint32_t>(m_disasm_count->value());
    const bool thumb = m_disasm_arch->currentText() == QStringLiteral("THUMB");

    CPUState &cpu = *emuenv.kernel.threads.begin()->second->cpu;

    QString output;
    uint16_t size = 1;
    uint32_t addr = address;

    for (uint32_t i = 0; i < count && size != 0; i++) {
        const size_t addr_page = addr / KiB(4);
        if (addr_page == 0 || !is_valid_addr(emuenv.mem, static_cast<Address>(addr_page * KiB(4)))) {
            output += tr("Disassembled %1 instructions.").arg(i);
            break;
        }

        std::string disasm = fmt::format("{:0>8X}: {}",
            addr, disassemble(cpu, addr, thumb, &size));
        output += QString::fromStdString(disasm);
        output += QLatin1Char('\n');
        addr += size;
    }

    m_disasm_output->setPlainText(output);
}
