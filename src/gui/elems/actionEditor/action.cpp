/* -----------------------------------------------------------------------------
 *
 * Giada - Your Hardcore Loopmachine
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2017 Giovanni A. Zuliani | Monocasual
 *
 * This file is part of Giada - Your Hardcore Loopmachine.
 *
 * Giada - Your Hardcore Loopmachine is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * Giada - Your Hardcore Loopmachine is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Giada - Your Hardcore Loopmachine. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------- */


#include <FL/fl_draw.H>
#include "../../../core/const.h"
#include "../../../core/mixer.h"
#include "../../../core/sampleChannel.h"
#include "../../../utils/log.h"
#include "../../dialogs/gd_actionEditor.h"
#include "actionEditor.h"
#include "action.h"


extern Mixer    G_Mixer;
extern Recorder G_Recorder;


/** TODO - index is useless?
 *  TODO - pass a record::action pointer and let geAction compute values */

geAction::geAction(int X, int Y, int H, int frame_a, unsigned index,
  gdActionEditor *parent, SampleChannel *ch, bool record, char type)
: Fl_Box     (X, Y, MIN_WIDTH, H),
  selected   (false),
  index      (index),
  parent     (parent),
  ch         (ch),
  type       (type),
  frame_a    (frame_a),
  onRightEdge(false),
  onLeftEdge (false)
{
	/* bool 'record' defines how to understand the action.
	 * record = false: don't record it, just show it. It happens when you
	 * open the editor with some actions to be shown.
	 *
	 * record = true: record it AND show it. It happens when you click on
	 * an empty area in order to add a new actions. First you record it
	 * (addAction()) then you show it (FLTK::Draw()) */

	if (record)
		addAction();

	/* in order to show a singlepress action we must compute the frame_b. We
	 * do that after the possible recording, otherwise we don't know which
	 * key_release is associated. */

	if (ch->mode == SINGLE_PRESS && type == ACTION_KEYPRESS) {
		Recorder::action *a2 = nullptr;
		G_Recorder.getNextAction(ch->index, ACTION_KEYREL, frame_a, &a2);
		if (a2) {
			frame_b = a2->frame;
			w((frame_b - frame_a)/parent->zoom);
		}
		else
			gu_log("[geActionEditor] frame_b not found! [%d:???]\n", frame_a);

	/* a singlepress action narrower than 8 pixel is useless. So check it.
	 * Warning: if an action is 8 px narrow, it has no body space to drag
	 * it. It's up to the user to zoom in and drag it. */

		if (w() < MIN_WIDTH)
			size(MIN_WIDTH, h());
	}
}


/* -------------------------------------------------------------------------- */


void geAction::draw()
{
	int color;
	if (selected)  /// && geActionEditor !disabled
		color = COLOR_BD_1;
	else
		color = COLOR_BG_2;

	if (ch->mode == SINGLE_PRESS) {
		fl_rectf(x(), y(), w(), h(), (Fl_Color) color);
	}
	else {
		if (type == ACTION_KILLCHAN)
			fl_rect(x(), y(), MIN_WIDTH, h(), (Fl_Color) color);
		else {
			fl_rectf(x(), y(), MIN_WIDTH, h(), (Fl_Color) color);
			if (type == ACTION_KEYPRESS)
				fl_rectf(x()+3, y()+h()-11, 2, 8, COLOR_BD_0);
			else
			if  (type == ACTION_KEYREL)
				fl_rectf(x()+3, y()+3, 2, 8, COLOR_BD_0);
		}
	}

}


/* -------------------------------------------------------------------------- */


int geAction::handle(int e)
{
	/* ret = 0 sends the event to the parent window. */

	int ret = 0;

	switch (e) {

		case FL_ENTER: {
			selected = true;
			ret = 1;
			redraw();
			break;
		}
		case FL_LEAVE: {
			fl_cursor(FL_CURSOR_DEFAULT, FL_WHITE, FL_BLACK);
			selected = false;
			ret = 1;
			redraw();
			break;
		}
		case FL_MOVE: {

			/* handling of the two margins, left & right. 4 pixels are good enough */

			if (ch->mode == SINGLE_PRESS) {
				onLeftEdge  = false;
				onRightEdge = false;
				if (Fl::event_x() >= x() && Fl::event_x() < x()+4) {
					onLeftEdge = true;
					fl_cursor(FL_CURSOR_WE, FL_WHITE, FL_BLACK);
				}
				else
				if (Fl::event_x() >= x()+w()-4 && Fl::event_x() <= x()+w()) {
					onRightEdge = true;
					fl_cursor(FL_CURSOR_WE, FL_WHITE, FL_BLACK);
				}
				else
					fl_cursor(FL_CURSOR_DEFAULT, FL_WHITE, FL_BLACK);
			}
		}
	}

	return ret;
}


/* -------------------------------------------------------------------------- */


void geAction::addAction()
{
	/* always check frame parity */

	if (frame_a % 2 != 0)
		frame_a++;

	/* anatomy of an action
	 * ____[#######]_____ (a) is the left margin, ACTION_KEYPRESS. (b) is
	 *     a       b      the right margin, the ACTION_KEYREL. This is the
	 * theory behind the singleshot.press actions; for any other kind the
	 * (b) is just a graphical and meaningless point. */

	if (ch->mode == SINGLE_PRESS) {
		G_Recorder.rec(parent->chan->index, ACTION_KEYPRESS, frame_a);
		G_Recorder.rec(parent->chan->index, ACTION_KEYREL, frame_a+4096);
		//gu_log("action added, [%d, %d]\n", frame_a, frame_a+4096);
	}
	else {
		G_Recorder.rec(parent->chan->index, parent->getActionType(), frame_a);
		//gu_log("action added, [%d]\n", frame_a);
	}

  parent->chan->hasActions = true;

	G_Recorder.sortActions();

	index++; // important!
}


/* -------------------------------------------------------------------------- */


void geAction::delAction()
{
	/* if SINGLE_PRESS you must delete both the keypress and the keyrelease
	 * actions. */

	if (ch->mode == SINGLE_PRESS) {
		G_Recorder.deleteAction(parent->chan->index, frame_a, ACTION_KEYPRESS,
      false, &G_Mixer.mutex_recs);
		G_Recorder.deleteAction(parent->chan->index, frame_b, ACTION_KEYREL,
      false, &G_Mixer.mutex_recs);
	}
	else
		G_Recorder.deleteAction(parent->chan->index, frame_a, type, false,
      &G_Mixer.mutex_recs);

  parent->chan->hasActions = G_Recorder.hasActions(parent->chan->index);


	/* restore the initial cursor shape, in case you delete an action and
	 * the double arrow (for resizing) is displayed */

	fl_cursor(FL_CURSOR_DEFAULT, FL_WHITE, FL_BLACK);
}


/* -------------------------------------------------------------------------- */


void geAction::moveAction(int frame_a)
{
	/* easy one: delete previous action and record the new ones. As usual,
	 * SINGLE_PRESS requires two jobs. If frame_a is valid, use that frame
	 * value. */

	delAction();

	if (frame_a != -1)
		this->frame_a = frame_a;
	else
		this->frame_a = xToFrame_a();


	/* always check frame parity */

	if (this->frame_a % 2 != 0)
		this->frame_a++;

	G_Recorder.rec(parent->chan->index, type, this->frame_a);

	if (ch->mode == SINGLE_PRESS) {
		frame_b = xToFrame_b();
		G_Recorder.rec(parent->chan->index, ACTION_KEYREL, frame_b);
	}

  parent->chan->hasActions = true;

	G_Recorder.sortActions();
}


/* -------------------------------------------------------------------------- */


int geAction::absx()
{
	return x() - parent->ac->x();
}


/* -------------------------------------------------------------------------- */


int geAction::xToFrame_a()
{
	return (absx()) * parent->zoom;
}


/* -------------------------------------------------------------------------- */


int geAction::xToFrame_b()
{
	return (absx() + w()) * parent->zoom;
}
