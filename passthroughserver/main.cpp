/* * This file is part of meego-im-framework *
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 * Contact: Nokia Corporation (directui@nokia.com)
 *
 * If you have questions regarding the use of this file, please contact
 * Nokia at directui@nokia.com.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * and appearing in the file LICENSE.LGPL included in the packaging
 * of this file.
 */


// Input method overlay window

#include <stdlib.h>

#include <QtDebug>
#include <QWidget>
#include <QPalette>

#include <mreactionmap.h>

#include <MApplication>
#include <MScene>
#include <mplainwindow.h>
#include <QCommonStyle>

#include "mimpluginmanager.h"
#include "mpassthruwindow.h"
#include "mimapplication.h"
#include "mimdummyinputcontext.h"

namespace {
    void disableMInputContextPlugin()
    {
        // prevent loading of minputcontext because we don't need it and starting
        // it might trigger starting of this service by the d-bus. not nice if that is
        // already happening :)
        if (-1 == unsetenv("QT_IM_MODULE")) {
            qWarning("meego-im-uiserver: unable to unset QT_IM_MODULE.");
        }

        MApplication::setLoadMInputContext(false);

        // TODO: Check if hardwiring the QStyle can be removed at a later stage.
        QApplication::setStyle(new QCommonStyle);
    }
}

int main(int argc, char **argv)
{
    bool bypassWMHint = false;
    bool selfComposited = false;

    for (int i = 1; i < argc; i++) {
        QString s(argv[i]);
        if (s == "-bypass-wm-hint") {
            bypassWMHint = true;
        }

        if (s == "-use-self-composition") {
            selfComposited = true;
        }
    }

    // QT_IM_MODULE, MApplication and QtMaemo5Style all try to load
    // MInputContext, which is fine for the application. For the passthrough
    // server itself, we absolutely need to prevent that.
    disableMInputContextPlugin();

    MIMApplication app(argc, argv, selfComposited);
    // Set a dummy input context so that Qt does not create a default input
    // context (qimsw-multi) which is expensive and not required by
    // meego-im-uiserver.
    app.setInputContext(new MIMDummyInputContext);

    selfComposited = app.supportsSelfComposite();

    MPassThruWindow widget(bypassWMHint, selfComposited);
    widget.setFocusPolicy(Qt::NoFocus);
    app.setPassThruWindow(&widget);

    // Must be declared after creation of top level window.
    MReactionMap reactionMap(&widget);
    MPlainWindow *view = new MPlainWindow(&widget);

#ifndef M_IM_DISABLE_TRANSLUCENCY
    if (!selfComposited)
    // enable translucent in hardware rendering
        view->setTranslucentBackground(!MApplication::softwareRendering());
#endif

    // No auto fill in software rendering
    if (MApplication::softwareRendering())
        view->viewport()->setAutoFillBackground(false);

    if (selfComposited) {
        widget.setAttribute(Qt::WA_NoSystemBackground);
        widget.setAttribute(Qt::WA_OpaquePaintEvent);
        view->setAttribute(Qt::WA_NoSystemBackground);
        view->setAttribute(Qt::WA_OpaquePaintEvent);
        view->viewport()->setAttribute(Qt::WA_NoSystemBackground);
        view->viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    }

    QSize sceneSize = view->visibleSceneSize(M::Landscape);
    int w = sceneSize.width();
    int h = sceneSize.height();
    view->setSceneRect(0, 0, w, h);

    widget.resize(sceneSize);

    view->setMinimumSize(1, 1);
    view->setMaximumSize(w, h);

    MIMPluginManager *pluginManager = new MIMPluginManager();

    QObject::connect(pluginManager, SIGNAL(regionUpdated(const QRegion &)),
                     &widget, SLOT(inputPassthrough(const QRegion &)));
#if defined(M_IM_DISABLE_TRANSLUCENCY) && !defined(M_IM_USE_SHAPE_WINDOW)
    QObject::connect(pluginManager, SIGNAL(regionUpdated(const QRegion &)),
                     view, SLOT(updatePosition(const QRegion &)));
#endif
    // hide active plugins when remote input window is gone or iconified.
    QObject::connect(&app, SIGNAL(remoteWindowGone()),
                     pluginManager, SLOT(hideActivePlugins()));

    return app.exec();
}

