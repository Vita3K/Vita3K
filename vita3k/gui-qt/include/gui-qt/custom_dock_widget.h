// credit to RPCS3
// https://github.com/RPCS3/rpcs3/blob/master/rpcs3/rpcs3qt/custom_dock_widget.h
#pragma once

#include <QDockWidget>
#include <QPainter>
#include <QStyleOption>

class custom_dock_widget : public QDockWidget {
public:
    explicit custom_dock_widget(const QString &title, QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags())
        : QDockWidget(title, parent, flags) {
        m_title_bar_widget = titleBarWidget();
        m_dummy_widget = new QWidget(this);
        m_dummy_widget->setVisible(false);

        connect(this, &QDockWidget::topLevelChanged, [this](bool /*top_level*/) {
            set_title_bar_visible(m_is_title_bar_visible);
            style()->unpolish(this);
            style()->polish(this);
        });
    }

    void set_title_bar_visible(bool visible) {
        if (visible || isFloating()) {
            if (m_title_bar_widget != titleBarWidget()) {
                setTitleBarWidget(m_title_bar_widget);
                QMargins margins = widget()->contentsMargins();
                margins.setTop(0);
                widget()->setContentsMargins(margins);
            }
        } else {
            setTitleBarWidget(m_dummy_widget);
            QMargins margins = widget()->contentsMargins();
            margins.setTop(1);
            widget()->setContentsMargins(margins);
        }

        m_is_title_bar_visible = visible;
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        // We need to repaint the dock widgets as plain widgets in floating mode.
        // Source: https://stackoverflow.com/questions/10272091/cannot-add-a-background-image-to-a-qdockwidget
        if (isFloating()) {
            QStyleOption opt;
            opt.initFrom(this);
            QPainter p(this);
            style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
            return;
        }

        // Use inherited method for docked mode because otherwise the dock would lose the title etc.
        QDockWidget::paintEvent(event);
    }

private:
    QWidget *m_title_bar_widget = nullptr;
    QWidget *m_dummy_widget = nullptr;
    bool m_is_title_bar_visible = true;
};
