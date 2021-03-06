/*
    Copyright (C) 2012 George Kiagiadakis <kiagiadakis.george@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "call-manager.h"
#include "call-window.h"
#include "approver.h"
#include "../libktpcall/call-channel-handler.h"

#include <KDebug>
#include <KTp/telepathy-handler-application.h>

struct CallManager::Private
{
    Tp::CallChannelPtr callChannel;
    CallChannelHandler *channelHandler;
    QWeakPointer<CallWindow> callWindow;
    QWeakPointer<Approver> approver;
};

CallManager::CallManager(const Tp::CallChannelPtr & callChannel, QObject *parent)
    : QObject(parent), d(new Private)
{
    KTp::TelepathyHandlerApplication::newJob();

    d->callChannel = callChannel;
    connect(callChannel.data(), SIGNAL(callStateChanged(Tp::CallState)),
            SLOT(onCallStateChanged(Tp::CallState)));

    //create the channel handler
    d->channelHandler = new CallChannelHandler(callChannel, this);

    //delete the CallManager when the channel has closed
    //and the farstream side has safely shut down.
    //NOTE this MUST be used with Qt::QueuedConnection because of
    // https://bugreports.qt-project.org/browse/QTBUG-24571
    connect(d->channelHandler, SIGNAL(channelClosed()),
            this, SLOT(deleteLater()), Qt::QueuedConnection);

    //bring us up-to-date with the current call state
    onCallStateChanged(d->callChannel->callState());
}

CallManager::~CallManager()
{
    kDebug() << "Deleting CallManager";

    //delete the window just in case CallManager was deleted
    //before the channel entered CallStateEnded
    delete d->callWindow.data();
    delete d;

    KTp::TelepathyHandlerApplication::jobFinished();
}

void CallManager::onCallStateChanged(Tp::CallState state)
{
    kDebug() << "new call state:" << state;

    switch (state) {
    case Tp::CallStatePendingInitiator:
        Q_ASSERT(d->callChannel->isRequested());
        (void) d->callChannel->accept();
        break;
    case Tp::CallStateInitialising:
        if (d->callChannel->isRequested()) {
            //show status that the call is conneting
            ensureCallWindow();
            d->callWindow.data()->setStatus(CallWindow::StatusConnecting);
        } else {
            kDebug() << "Call is initialising";
        }
        break;
    case Tp::CallStateInitialised:
        if (d->callChannel->isRequested()) {
            //show status that the remote end is ringing
            ensureCallWindow();
            d->callWindow.data()->setStatus(CallWindow::StatusRemoteRinging);
        } else {
            //show approver
            d->approver = new Approver(d->callChannel, this);
            connect(d->approver.data(), SIGNAL(channelAccepted()), SLOT(onCallAccepted()));
            connect(d->approver.data(), SIGNAL(channelRejected()), SLOT(onCallRejected()));
            (void) d->callChannel->setRinging();
        }
        break;
    case Tp::CallStateAccepted:
        if (d->callChannel->isRequested()) {
            //show status that the remote end accepted the call
            ensureCallWindow();
            d->callWindow.data()->setStatus(CallWindow::StatusRemoteAccepted);
        } else {
            //hide approver & show call window
            delete d->approver.data();
            ensureCallWindow();
            d->callWindow.data()->setStatus(CallWindow::StatusConnecting);
        }
        break;
    case Tp::CallStateActive:
        //normally the approver is already deleted and the call window
        //already exists at this point, but we just want to be safe
        //in case the CM decides to do a weird state jump
        if (!d->callChannel->isRequested()) {
            delete d->approver.data();
        }
        ensureCallWindow();
        d->callWindow.data()->setStatus(CallWindow::StatusActive);
        break;
    case Tp::CallStateEnded:
        //if we requested the call, make sure we have a window to show the error (if any)
        if (d->callChannel->isRequested()) {
            ensureCallWindow();
        }

        if (d->callWindow) {
            Tp::CallStateReason reason = d->callChannel->callStateReason();
            d->callWindow.data()->setStatus(CallWindow::StatusDisconnected, reason);

            //kill the call manager when the call window is closed,
            //after shutting down the channelHandler
            connect(d->callWindow.data(), SIGNAL(destroyed()), d->channelHandler, SLOT(shutdown()));
        } else {
            //missed the call
            kDebug() << "missed call";
            delete d->approver.data();
            d->channelHandler->shutdown();
        }
        break;
    default:
        Q_ASSERT(false);
    }
}

void CallManager::onCallAccepted()
{
    (void) d->callChannel->accept();
}

void CallManager::onCallRejected()
{
    (void) d->callChannel->hangup(Tp::CallStateChangeReasonRejected, TP_QT_ERROR_REJECTED);
}

void CallManager::ensureCallWindow()
{
    if (!d->callWindow) {
        d->callWindow = new CallWindow(d->callChannel);
        d->callWindow.data()->show();
        d->callWindow.data()->setAttribute(Qt::WA_DeleteOnClose);

        connect(d->channelHandler, SIGNAL(contentAdded(CallContentHandler*)),
                d->callWindow.data(), SLOT(onContentAdded(CallContentHandler*)));
        connect(d->channelHandler, SIGNAL(contentRemoved(CallContentHandler*)),
                d->callWindow.data(), SLOT(onContentRemoved(CallContentHandler*)));

        //inform the ui about already existing contents
        Q_FOREACH(CallContentHandler *content, d->channelHandler->contents()) {
            d->callWindow.data()->onContentAdded(content);
        }
    }
}
