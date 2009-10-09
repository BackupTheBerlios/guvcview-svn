/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#                                                                               #
# This program is free software; you can redistribute it and/or modify          #
# it under the terms of the GNU General Public License as published by          #
# the Free Software Foundation; either version 2 of the License, or             #
# (at your option) any later version.                                           #
#                                                                               #
# This program is distributed in the hope that it will be useful,               #
# but WITHOUT ANY WARRANTY; without even the implied warranty of                #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 #
# GNU General Public License for more details.                                  #
#                                                                               #
# You should have received a copy of the GNU General Public License             #
# along with this program; if not, write to the Free Software                   #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA     #
#                                                                               #
********************************************************************************/

/* support for internationalization - i18n */
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "img_controls.h"
#include "v4l2uvc.h"
#include "v4l2_dyna_ctrls.h"
#include "globals.h"
#include "string_utils.h"
#include "autofocus.h"
#include "callbacks.h"

/*exposure menu for old type controls */
// static const char *exp_typ[]={
				// "Manual Mode",
				// "Auto Mode",
				// "Shutter Priority Mode",
				// "Aperture Priority Mode"
				// };

/*--------------------------- draw camera controls ---------------------------*/
void
draw_controls (struct ALL_DATA *all_data)
{
	int i=0;
	struct VidState *s = all_data->s;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	//add dynamic controls
	//only uvc logitech cameras
	//needs admin rights
	//only for uvc driver (uvcvideo)
	if(videoIn->listDevices->num_devices > 0)
	{
		g_printf("vid:%04x \npid:%04x \ndriver:%s\n",
			videoIn->listDevices->listVidDevices[videoIn->listDevices->current_device].vendor,
			videoIn->listDevices->listVidDevices[videoIn->listDevices->current_device].product,
			videoIn->listDevices->listVidDevices[videoIn->listDevices->current_device].driver);
		if(g_strcmp0(videoIn->listDevices->listVidDevices[videoIn->listDevices->current_device].driver,"uvcvideo") == 0)
		{
			if(videoIn->listDevices->listVidDevices[videoIn->listDevices->current_device].vendor != 0)
			{
				//check for logitech vid
				if (videoIn->listDevices->listVidDevices[videoIn->listDevices->current_device].vendor == 0x046d)
					initDynCtrls(videoIn->fd);
			}
			else initDynCtrls(videoIn->fd);
		}
	}
	
	if (s->control) 
	{
		input_free_controls (s);
	}
	s->num_controls = 0;
	s->control = input_enum_controls (videoIn->fd, &(s->num_controls));
	
	/*              try to start the video stream             */
	/* do it here (after all ioctls) since some cameras take  */
	/* a long time to initialize after this                   */
	/* it's OK if it fails since it is retried in uvcGrab     */
	video_enable(videoIn);
	
	if (global->debug) 
	{
		g_printf("Controls:\n");
		for (i = 0; i < s->num_controls; i++)
		{
			g_printf("control[%d]: 0x%x",i,s->control[i].id);
			g_printf ("  %s, %d:%d:%d, default %d\n", s->control[i].name,
				s->control[i].min, s->control[i].step, s->control[i].max,
				s->control[i].default_val);
		}
	}
	s->control_info = g_new0(ControlInfo, s->num_controls);

	int row=0;

	for (i = 0; i < s->num_controls; i++) 
	{
		ControlInfo * ci = s->control_info + i;
		InputControl * c = s->control + i;

		ci->idx = i;
		ci->widget = NULL;
		ci->label = NULL;
		ci->spinbutton = NULL;
		
		if (c->id == V4L2_CID_PAN_RELATIVE) 
		{
			videoIn->PanTilt++;
		
			ci->widget = gtk_vbox_new (FALSE, 0);
			GtkWidget *panHbox = gtk_hbox_new (FALSE, 0);
			GtkWidget *reversePan = gtk_check_button_new_with_label (_("Invert (Pan)"));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (reversePan),
				(global->PanStep < 0) ? TRUE : FALSE);

			GtkWidget *PanLeft = gtk_button_new_with_label(_("Left"));
			
			g_object_set_data (G_OBJECT (PanLeft), "pan_info", GINT_TO_POINTER(-INCPANTILT));
			g_object_set_data (G_OBJECT (PanLeft), "tilt_info", GINT_TO_POINTER(0));
			g_object_set_data (G_OBJECT (PanLeft), "reset_info", GINT_TO_POINTER(0));
			
			GtkWidget *PanRight = gtk_button_new_with_label(_("Right"));

			g_object_set_data (G_OBJECT (PanRight), "pan_info", GINT_TO_POINTER(INCPANTILT));
			g_object_set_data (G_OBJECT (PanRight), "tilt_info", GINT_TO_POINTER(0));
			g_object_set_data (G_OBJECT (PanRight), "reset_info", GINT_TO_POINTER(0));
			
			gtk_box_pack_start (GTK_BOX (panHbox), PanLeft, TRUE, TRUE, 0);
			gtk_box_pack_start (GTK_BOX (panHbox), PanRight, TRUE, TRUE, 0);
			
			gtk_box_pack_start (GTK_BOX (ci->widget), reversePan, TRUE, TRUE, 0);
			gtk_box_pack_start (GTK_BOX (ci->widget), panHbox, TRUE, TRUE, 0);
			
			gtk_widget_show (reversePan);
			gtk_widget_show (PanLeft);
			gtk_widget_show (PanRight);
			gtk_widget_show(panHbox);
			
			g_signal_connect (G_OBJECT (reversePan), "toggled",
				G_CALLBACK (reversePan_changed), all_data);
			g_signal_connect (GTK_BUTTON (PanLeft), "clicked",
				G_CALLBACK (PanTilt_clicked), all_data);
			g_signal_connect (GTK_BUTTON (PanRight), "clicked",
				G_CALLBACK (PanTilt_clicked), all_data);
			
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
				GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);
			gchar *tmp;
			tmp =  g_strdup_printf ("%s:", gettext(c->name));
			ci->label = gtk_label_new (tmp);
			g_free(tmp);
			
		}
		else if (c->id == V4L2_CID_TILT_RELATIVE)
		{
			videoIn->PanTilt++; 
			
			ci->widget = gtk_hbox_new (FALSE, 0);
			GtkWidget *TiltUp = gtk_button_new_with_label(_("Up"));
			GtkWidget *TiltDown = gtk_button_new_with_label(_("Down"));
			gtk_box_pack_start (GTK_BOX (ci->widget), TiltUp, TRUE, TRUE, 0);
			gtk_box_pack_start (GTK_BOX (ci->widget), TiltDown, TRUE, TRUE, 0);
			gtk_widget_show (TiltUp);
			gtk_widget_show (TiltDown);
			// Tilt Up
			g_object_set_data (G_OBJECT (TiltUp), "pan_info", GINT_TO_POINTER(0));
			g_object_set_data (G_OBJECT (TiltUp), "tilt_info", GINT_TO_POINTER(-INCPANTILT));
			g_object_set_data (G_OBJECT (TiltUp), "reset_info", GINT_TO_POINTER(0));
			//Tilt Down
			g_object_set_data (G_OBJECT (TiltDown), "pan_info", GINT_TO_POINTER(0));
			g_object_set_data (G_OBJECT (TiltDown), "tilt_info", GINT_TO_POINTER((INCPANTILT)));
			g_object_set_data (G_OBJECT (TiltDown), "reset_info", GINT_TO_POINTER(0));
			
			g_signal_connect (GTK_BUTTON (TiltUp), "clicked",
				G_CALLBACK (PanTilt_clicked), all_data);
			g_signal_connect (GTK_BUTTON (TiltDown), "clicked",
				G_CALLBACK (PanTilt_clicked), all_data);
			
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
				GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			//g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);
			gchar *tmp;
			tmp = g_strdup_printf ("%s:", gettext(c->name));
			ci->label = gtk_label_new (tmp);
			g_free(tmp);
			
		} 
		else if (c->id == V4L2_CID_PAN_RESET) 
		{
			videoIn->PanTilt++; //Pan reset
		
			ci->widget = gtk_button_new_with_label(_("Reset"));

			g_object_set_data (G_OBJECT (ci->widget), "pan_info", GINT_TO_POINTER(0));
			g_object_set_data (G_OBJECT (ci->widget), "tilt_info", GINT_TO_POINTER((0)));
			g_object_set_data (G_OBJECT (ci->widget), "reset_info", GINT_TO_POINTER(1));
			
			g_signal_connect (GTK_BUTTON (ci->widget), "clicked",
				G_CALLBACK (PanTilt_clicked), all_data);
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
				GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);
			gchar *tmp;
			tmp = g_strdup_printf ("%s:", gettext(c->name));
			ci->label = gtk_label_new (tmp);
			g_free(tmp);
		
		}
		else if (c->id == V4L2_CID_TILT_RESET) 
		{
			videoIn->PanTilt++; //Pan reset
		
			ci->widget = gtk_button_new_with_label(_("Reset"));
		
			g_object_set_data (G_OBJECT (ci->widget), "pan_info", GINT_TO_POINTER(0));
			g_object_set_data (G_OBJECT (ci->widget), "tilt_info", GINT_TO_POINTER((0)));
			g_object_set_data (G_OBJECT (ci->widget), "reset_info", GINT_TO_POINTER(2));
			
			g_signal_connect (GTK_BUTTON (ci->widget), "clicked",
				G_CALLBACK (PanTilt_clicked), all_data);
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
				GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			
			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);
			gchar *tmp;
			tmp = g_strdup_printf ("%s:", gettext(c->name));
			ci->label = gtk_label_new (tmp);
			g_free(tmp);
		
		}
		else if (c->id == V4L2_CID_PANTILT_RESET_LOGITECH)
		{
			videoIn->PanTilt++;
			
			ci->widget = gtk_button_new_with_label(_("Reset"));
			
			g_object_set_data (G_OBJECT (ci->widget), "pan_info", GINT_TO_POINTER(0));
			g_object_set_data (G_OBJECT (ci->widget), "tilt_info", GINT_TO_POINTER((0)));
			g_object_set_data (G_OBJECT (ci->widget), "reset_info", GINT_TO_POINTER(3));
			
			g_signal_connect (GTK_BUTTON (ci->widget), "clicked",
				G_CALLBACK (PanTilt_clicked), all_data);
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
				GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);
			gchar *tmp;
			tmp = g_strdup_printf ("%s:", gettext(c->name));
			ci->label = gtk_label_new (tmp);
			g_free(tmp);
		
		} 
		else if (c->type == INPUT_CONTROL_TYPE_INTEGER) 
		{
			int val=0;

			if (c->step == 0)
				c->step = 1;
			ci->widget = gtk_hscale_new_with_range (c->min, c->max, c->step);
			gtk_scale_set_draw_value (GTK_SCALE (ci->widget), FALSE);

			/* This is a hack to use always round the HScale to integer
			 * values.  Strangely, this functionality is normally only
			 * available when draw_value is TRUE. */
			GTK_RANGE (ci->widget)->round_digits = 0;

			g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);
			
			ci->spinbutton = gtk_spin_button_new_with_range(c->min,c->max,c->step);
			g_object_set_data (G_OBJECT (ci->spinbutton), "control_info", ci);
			
			/*can't edit the spin value by hand*/
			gtk_editable_set_editable(GTK_EDITABLE(ci->spinbutton),FALSE);

			if (input_get_control (videoIn->fd, c, &val) == 0) 
			{
				gtk_range_set_value (GTK_RANGE (ci->widget), val);
				gtk_spin_button_set_value (GTK_SPIN_BUTTON(ci->spinbutton), val);
			}
			else 
			{
				/*couldn't get control value -> set to default*/
				input_set_control (videoIn->fd, c, c->default_val);
				gtk_range_set_value (GTK_RANGE (ci->widget), c->default_val);
				gtk_spin_button_set_value (GTK_SPIN_BUTTON(ci->spinbutton), c->default_val);
				gtk_widget_set_sensitive (ci->widget, TRUE);
				gtk_widget_set_sensitive (ci->spinbutton, TRUE);
			}

			if (!c->enabled) 
			{
				gtk_widget_set_sensitive (ci->widget, FALSE);
				gtk_widget_set_sensitive (ci->spinbutton, FALSE);
			}
			
			g_signal_connect (G_OBJECT (ci->widget), "value-changed",
				G_CALLBACK (slider_changed), all_data);
			g_signal_connect (G_OBJECT (ci->spinbutton),"value-changed",
				G_CALLBACK (spin_changed), all_data);

			gtk_widget_show (ci->spinbutton);
			gchar *tmp;
			tmp = g_strdup_printf ("%s:", gettext(c->name));
			ci->label = gtk_label_new (tmp);
			g_free(tmp);
			/* ---- Add auto-focus checkbox and focus button ----- */
			if (((c->id== V4L2_CID_FOCUS_ABSOLUTE) || (c->id== V4L2_CID_FOCUS_LOGITECH)) && !(global->control_only)) 
			{
				global->AFcontrol=1;
				GtkWidget *Focus_box = gtk_hbox_new (FALSE, 0);
				GtkWidget *AutoFocus = gtk_check_button_new_with_label (_("Auto Focus (continuous)"));
				GtkWidget *FocusButton = gtk_button_new_with_label (_("set Focus"));
				gtk_box_pack_start (GTK_BOX (Focus_box), AutoFocus, TRUE, TRUE, 0);
				gtk_box_pack_start (GTK_BOX (Focus_box), FocusButton, TRUE, TRUE, 0);
				gtk_widget_show (Focus_box);
				gtk_widget_show (AutoFocus);
				gtk_widget_show (FocusButton);
				gtk_table_attach (GTK_TABLE (s->table), Focus_box, 1, 2, 3+row, 4+row,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (AutoFocus),
					global->autofocus ? TRUE: FALSE);

				g_object_set_data (G_OBJECT (AutoFocus), "control_info", ci);
				g_object_set_data (G_OBJECT (FocusButton), "control_info", ci);
				
				g_signal_connect (G_OBJECT (AutoFocus), "toggled",
					G_CALLBACK (autofocus_changed), all_data);
				g_signal_connect (G_OBJECT (FocusButton), "clicked",
					G_CALLBACK (setfocus_clicked), all_data);
				row++; /*increment control row*/
			
			}
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
				GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
		
			gtk_table_attach (GTK_TABLE (s->table), ci->spinbutton, 2, 3,
				3+row, 4+row, GTK_SHRINK | GTK_FILL, 0, 0, 0);
		
		} 
		else if(c->id ==V4L2_CID_DISABLE_PROCESSING_LOGITECH) 
		{
			int val=0;
			ci->widget = gtk_vbox_new (FALSE, 0);
			GtkWidget *check_bayer = gtk_check_button_new_with_label (gettext(c->name));
			g_object_set_data (G_OBJECT (check_bayer), "control_info", ci);
			GtkWidget *pix_ord = gtk_combo_box_new_text ();
			gtk_combo_box_append_text(GTK_COMBO_BOX(pix_ord),"GBGB... | RGRG...");
			gtk_combo_box_append_text(GTK_COMBO_BOX(pix_ord),"GRGR... | BGBG...");
			gtk_combo_box_append_text(GTK_COMBO_BOX(pix_ord),"BGBG... | GRGR...");
			gtk_combo_box_append_text(GTK_COMBO_BOX(pix_ord),"RGRG... | GBGB...");
			/* auto set pix order for 2Mp logitech cameras */
			if((videoIn->width == 160) || (videoIn->width == 176) || 
				(videoIn->width == 352) || (videoIn->width == 960)) 
					videoIn->pix_order=3; /* rg */
			else videoIn->pix_order=0; /* gb */

			gtk_combo_box_set_active(GTK_COMBO_BOX(pix_ord),videoIn->pix_order);
			
			gtk_box_pack_start (GTK_BOX (ci->widget), check_bayer, TRUE, TRUE, 0);
			gtk_box_pack_start (GTK_BOX (ci->widget), pix_ord, TRUE, TRUE, 0);
			gtk_widget_show (check_bayer);
			gtk_widget_show (pix_ord);
			g_signal_connect (GTK_TOGGLE_BUTTON (check_bayer), "toggled",
				G_CALLBACK (bayer_changed), all_data);
			g_signal_connect (GTK_COMBO_BOX (pix_ord), "changed",
				G_CALLBACK (pix_ord_changed), all_data);
			
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
				GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);
			gchar *tmp;
			tmp = g_strdup_printf (_("raw pixel order:"));
			ci->label = gtk_label_new (tmp);
			g_free(tmp);
			
			if (input_get_control (videoIn->fd, c, &val) == 0) 
			{
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_bayer),
					val ? TRUE : FALSE);
				if(val>0) 
				{
					if (global->debug) 
					{
						g_printf("bayer mode set\n");
					}
					videoIn->isbayer=1;
				}
			}

			if (!c->enabled) 
			{
				gtk_widget_set_sensitive (ci->widget, FALSE);
			}
		
		}
		else if (c->type == INPUT_CONTROL_TYPE_BOOLEAN) 
		{
			int val=0;
			ci->widget = gtk_check_button_new_with_label (gettext(c->name));
			g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			gtk_widget_show (ci->widget);
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 3, 3+row, 4+row,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

			if (input_get_control (videoIn->fd, c, &val) == 0) 
			{
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
					val ? TRUE : FALSE);
			}
			else 
			{
				/*couldn't get control value -> set to default*/
				input_set_control (videoIn->fd, c, c->default_val);
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
					c->default_val ? TRUE : FALSE);
				gtk_widget_set_sensitive (ci->widget, TRUE);
			}

			if (!c->enabled) 
			{
				gtk_widget_set_sensitive (ci->widget, FALSE);
			}
			
			g_signal_connect (G_OBJECT (ci->widget), "toggled",
				G_CALLBACK (check_changed), all_data);
		}
		else if (c->type == INPUT_CONTROL_TYPE_MENU) 
		{
			int val=0;
			int j=0;

			ci->widget = gtk_combo_box_new_text ();
			for (j = 0; j <= c->max; j++) 
			{
				gtk_combo_box_append_text (GTK_COMBO_BOX (ci->widget), gettext(c->entries[j]));
			}

			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
				GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			gtk_widget_show (ci->widget);

			if (input_get_control (videoIn->fd, c, &val) == 0) 
			{
				gtk_combo_box_set_active (GTK_COMBO_BOX (ci->widget), val);
			}
			else 
			{
				/*couldn't get control value -> set to default*/
				input_set_control (videoIn->fd, c, c->default_val);
				gtk_combo_box_set_active (GTK_COMBO_BOX (ci->widget), c->default_val);
				gtk_widget_set_sensitive (ci->widget, TRUE);
			}

			if (!c->enabled) 
			{
				gtk_widget_set_sensitive (ci->widget, FALSE);
			}
			
			g_signal_connect (G_OBJECT (ci->widget), "changed",
				G_CALLBACK (combo_changed), all_data);
			gchar *tmp;
			tmp = g_strdup_printf ("%s:", gettext(c->name));
			ci->label = gtk_label_new (tmp);
			g_free(tmp);
		}
		else 
		{
			g_printf ("TODO: implement button controls\n");
			continue;
		}

		if (ci->label) 
		{
			gtk_misc_set_alignment (GTK_MISC (ci->label), 1, 0.5);

			gtk_table_attach (GTK_TABLE (s->table), ci->label, 0, 1, 3+row, 4+row,
				GTK_FILL, 0, 0, 0);

			gtk_widget_show (ci->label);
		}
		row++; /*increment control row*/
	}
	
	s = NULL;
	global = NULL;
	videoIn = NULL;
}

