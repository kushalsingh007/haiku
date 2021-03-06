/*
 * Copyright 2003-2014, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Michael Phipps
 *		Jérôme Duval, jerome.duval@free.fr
 *		Ryan Leavengood, leavengood@gmail.com
 *		Puck Meerburg, puck@puckipedia.nl
 */


#include "ScreenBlanker.h"
#include "ScreenSaverWindow.h"

#include <Application.h>
#include <View.h>

#include <WindowPrivate.h>

#include <syslog.h>


//	#pragma mark - ScreenSaverFilter


/* This message filter is what will close the screensaver upon user activity. */
filter_result
ScreenSaverFilter::Filter(BMessage* message, BHandler** target)
{
	// This guard is used to avoid sending multiple B_QUIT_REQUESTED messages
	if (fEnabled) {
		switch (message->what) {
			case B_MOUSE_MOVED:
			{
				// ignore the initial B_MOUSE_MOVED sent by the app_server
				// in test mode, all mouse move events are ignored
				bool transitOnly = false;
				if (fTestMode
					|| message->FindBool("be:transit_only", &transitOnly) == B_OK
					&& transitOnly)
					return B_DISPATCH_MESSAGE;

				// Fall through
			}
			case B_KEY_DOWN:
			{
				// we ignore the Print-Screen key to make screen shots of
				// screen savers possible
				int32 key;
				if (message->FindInt32("key", &key) == B_OK && key == 0xe)
					return B_DISPATCH_MESSAGE;

				// Fall through
			}
			case B_MODIFIERS_CHANGED:
			case B_UNMAPPED_KEY_DOWN:
			case B_MOUSE_DOWN:
				fEnabled = false;
				be_app->PostMessage(B_QUIT_REQUESTED);
				break;
		}
	} else if (message->what == B_KEY_DOWN) {
		ScreenBlanker* app = dynamic_cast<ScreenBlanker*>(be_app);
		if (app != NULL && app->IsPasswordWindowShown()) {
			// Handle the escape key when the password window is showing
			const char* string = NULL;
			if (message->FindString("bytes", &string) == B_OK
					&& string[0] == B_ESCAPE) {
				be_app->PostMessage(kMsgResumeSaver);
			}
		}
	}

	return B_DISPATCH_MESSAGE;
}


//	#pragma mark - ScreenSaverWindow


/*!
	This is the BDirectWindow subclass that rendering occurs in.
	A view is added to it so that BView based screensavers will work.
*/
ScreenSaverWindow::ScreenSaverWindow(BRect frame, bool test)
	:
	BDirectWindow(frame, "ScreenSaver Window",
		B_NO_BORDER_WINDOW_LOOK, kWindowScreenFeel,
		B_NOT_RESIZABLE | B_NOT_MOVABLE | B_NOT_MINIMIZABLE
		| B_NOT_ZOOMABLE | B_NOT_CLOSABLE, B_ALL_WORKSPACES),
	fTopView(NULL),
	fSaverRunner(NULL),
	fFilter(NULL)
{
	frame.OffsetTo(0, 0);
	fTopView = new BView(frame, "ScreenSaver View", B_FOLLOW_ALL,
		B_WILL_DRAW);
	fTopView->SetViewColor(B_TRANSPARENT_COLOR);

	fFilter = new ScreenSaverFilter(test);
	fTopView->AddFilter(fFilter);

	AddChild(fTopView);

	// Ensure that this view receives keyboard and mouse input
	fTopView->MakeFocus(true);
	fTopView->SetEventMask(B_KEYBOARD_EVENTS | B_POINTER_EVENTS,
		B_NO_POINTER_HISTORY);
}


ScreenSaverWindow::~ScreenSaverWindow()
{
	Hide();
}


void
ScreenSaverWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgEnableFilter:
			fFilter->SetEnabled(true);
			break;

		default:
			BWindow::MessageReceived(message);
	}
}


bool
ScreenSaverWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}


void
ScreenSaverWindow::DirectConnected(direct_buffer_info* info)
{
	BScreenSaver* saver = _ScreenSaver();
	if (saver != NULL)
		saver->DirectConnected(info);
}


void
ScreenSaverWindow::SetSaverRunner(ScreenSaverRunner* runner)
{
	fSaverRunner = runner;
}


BScreenSaver*
ScreenSaverWindow::_ScreenSaver()
{
	if (fSaverRunner != NULL)
		return fSaverRunner->ScreenSaver();

	return NULL;
}
