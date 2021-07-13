/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2006, 2007 Kevin Ottens <ervin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kdialogjobuidelegate.h"

#include <QPointer>
#include <QQueue>
#include <QWidget>

#include <KJob>
#include <KMessageDialog>
#include <kjobwidgets.h>

#include <config-kjobwidgets.h>
#if HAVE_X11
#include <QX11Info>
#endif

struct MessageBoxData {
    QWidget *widget;
    KMessageDialog::Type type;
    QString msg;
};

class KDialogJobUiDelegatePrivate : public QObject
{
    Q_OBJECT
public:
    explicit KDialogJobUiDelegatePrivate(KDialogJobUiDelegate *qq);
    ~KDialogJobUiDelegatePrivate() override;
    void queuedMessageBox(QWidget *widget, KMessageDialog::Type type, const QString &msg);

    QWidget *window = nullptr;

public Q_SLOTS:
    void next();

private:
    bool running = false;
    QQueue<QSharedPointer<MessageBoxData>> queue;
};

KDialogJobUiDelegatePrivate::KDialogJobUiDelegatePrivate(KDialogJobUiDelegate *qq)
    : QObject(qq)
{
}

KDialogJobUiDelegatePrivate::~KDialogJobUiDelegatePrivate()
{
    queue.clear();
}

void KDialogJobUiDelegatePrivate::next()
{
    if (queue.isEmpty()) {
        running = false;
        return;
    }

    QSharedPointer<MessageBoxData> data = queue.dequeue();

    auto *dlg = new KMessageDialog(data->type, data->msg, data->widget);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModal(true);
    dlg->setButtons();

    connect(this, &QObject::destroyed, dlg, &QDialog::reject);

    connect(dlg, &QDialog::finished, window, [this]() {
        QMetaObject::invokeMethod(this, &KDialogJobUiDelegatePrivate::next, Qt::QueuedConnection);
    });

    dlg->show();
}

void KDialogJobUiDelegatePrivate::queuedMessageBox(QWidget *widget, KMessageDialog::Type type, const QString &msg)
{
    QSharedPointer<MessageBoxData> data(new MessageBoxData);
    data->type = type;
    data->widget = widget;
    data->msg = msg;

    queue.enqueue(data);

    if (!running) {
        running = true;
        QMetaObject::invokeMethod(this, &KDialogJobUiDelegatePrivate::next, Qt::QueuedConnection);
    }
}

KDialogJobUiDelegate::KDialogJobUiDelegate()
    : KJobUiDelegate()
    , d(new KDialogJobUiDelegatePrivate(this))
{
}

KDialogJobUiDelegate::KDialogJobUiDelegate(KJobUiDelegate::Flags flags, QWidget *window)
    : KJobUiDelegate(flags)
    , d(new KDialogJobUiDelegatePrivate(this))
{
    d->window = window;
}

KDialogJobUiDelegate::~KDialogJobUiDelegate() = default;

bool KDialogJobUiDelegate::setJob(KJob *job)
{
    bool ret = KJobUiDelegate::setJob(job);
#if HAVE_X11
    if (ret) {
        unsigned long time = QX11Info::appUserTime();
        KJobWidgets::updateUserTimestamp(job, time);
    }
#endif
    return ret;
}

void KDialogJobUiDelegate::setWindow(QWidget *window)
{
    if (job()) {
        KJobWidgets::setWindow(job(), window);
    }
    d->window = window;
}

QWidget *KDialogJobUiDelegate::window() const
{
    if (d->window) {
        return d->window;
    } else if (job()) {
        return KJobWidgets::window(job());
    }
    return nullptr;
}

void KDialogJobUiDelegate::updateUserTimestamp(unsigned long time)
{
    KJobWidgets::updateUserTimestamp(job(), time);
}

unsigned long KDialogJobUiDelegate::userTimestamp() const
{
    return KJobWidgets::userTimestamp(job());
}

void KDialogJobUiDelegate::showErrorMessage()
{
    if (job()->error() != KJob::KilledJobError) {
        d->queuedMessageBox(window(), KMessageDialog::Error, job()->errorString());
    }
}

void KDialogJobUiDelegate::slotWarning(KJob * /*job*/, const QString &plain, const QString & /*rich*/)
{
    if (isAutoWarningHandlingEnabled()) {
        d->queuedMessageBox(window(), KMessageDialog::Information, plain);
    }
}

#include "kdialogjobuidelegate.moc"
