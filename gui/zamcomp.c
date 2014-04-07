#include "../zamcomp.h"

#define MTR_URI "http://zamaudio.com/lv2/zamcomp#"
#define MTR_GUI "ui"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#define COMPOINTS 1500

#define LOGO_W (160.)
#define LOGO_H (30.)

#define PLOT_W (280.)
#define PLOT_H (280.)

typedef struct {
	LV2UI_Write_Function write;
	LV2UI_Controller controller;

	RobWidget *hbox, *ctable;

	RobTkLbl  *lbl_att;
	RobTkSpin *knob_att;
	RobTkLbl  *lbl_rel;
	RobTkSpin *knob_rel;
	RobTkLbl  *lbl_knee;
	RobTkSpin *knob_knee;
	RobTkLbl  *lbl_ratio;
	RobTkSpin *knob_ratio;
	RobTkLbl  *lbl_makeup;
	RobTkSpin *knob_makeup;
	RobTkLbl  *lbl_thresh;
	RobTkScale *slider_thresh;
	RobTkSep  *sep[3];

	//RobTkDarea *darea;
	RobTkXYp  *xyp;
	RobTkImg  *logo;

	cairo_surface_t *frontface;
	cairo_surface_t *compcurve;
	
	float compx[COMPOINTS];
	float compy[COMPOINTS];
	float knobs[6];

	bool disable_signals;
	bool first;

} ZamComp_UI;

static inline double
to_dB(double g) {
	return (20.*log10(g));
}

static inline double
from_dB(double gdb) {
	return (exp(gdb/20.*log(10.)));
}

static inline double
sanitize_denormal(double value) {
	if (!isnormal(value)) {
		return (0.);
	}
	return value;
}

static void calceqcurve(ZamComp_UI* ui, float x[], float y[])
{

	float knee = ui->knobs[2];
	float ratio = ui->knobs[3];
	float makeup = ui->knobs[4];
	float thresdb = ui->knobs[5];
	float width=((knee+1.f)-0.99f)*6.f;

	float xg, yg;
	float max_x = 8.f;
	float min_x = -60.f;

	for (int i = 0; i < COMPOINTS; ++i) {
		float xx,x2;
		xx = i;
		x2 = (max_x - min_x) / COMPOINTS * i + min_x;
		yg = 0.f;
		xg = (x2==0.f) ? -160.f : to_dB(fabs(x2));
		xg = sanitize_denormal(xg);

		if (2.f*(xg-thresdb)<-width) {
			yg = xg;
		} else if (2.f*fabs(xg-thresdb)<=width) {
			yg = xg + (1.f/ratio-1.f)*(xg-thresdb+width/2.f)*(xg-thresdb+width/2.f)/(2.f*width);
		} else if (2.f*(xg-thresdb)>width) {
			yg = thresdb + (xg-thresdb)/ratio;
		}

		yg = sanitize_denormal(yg);
		y[i] = from_dB(yg);
		x[i] = from_dB(xg);
	}
}

#include "gui/img/logo.c"

static bool expose_event(RobWidget* handle, cairo_t* cr, cairo_rectangle_t *ev)
{
	ZamComp_UI* ui = (ZamComp_UI*) GET_HANDLE(handle);

	/* limit cairo-drawing to exposed area */
	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip(cr);
	cairo_set_source_surface(cr, ui->frontface, 0, 0);
	cairo_paint (cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	return TRUE;
}

static void xy_clip_fn(cairo_t *cr, void *data)
{
	rounded_rectangle(cr, 10, 10, PLOT_W-20, PLOT_H-20, 10);
	cairo_clip(cr);
}

static bool cb_set_knobs (RobWidget* handle, void *data) {
	ZamComp_UI* ui = (ZamComp_UI*) (data);
	// continue polling until all signals read
	if (ui->disable_signals) return FALSE;
	
	ui->knobs[0] = robtk_spin_get_value(ui->knob_att);
	ui->write(ui->controller, ZAMCOMP_ATTACK, sizeof(float), 0, (const void*) &ui->knobs[0]);
	ui->knobs[1] = robtk_spin_get_value(ui->knob_rel);
	ui->write(ui->controller, ZAMCOMP_RELEASE, sizeof(float), 0, (const void*) &ui->knobs[1]);
	ui->knobs[2] = robtk_spin_get_value(ui->knob_knee);
	ui->write(ui->controller, ZAMCOMP_KNEE, sizeof(float), 0, (const void*) &ui->knobs[2]);
	ui->knobs[3] = robtk_spin_get_value(ui->knob_ratio);
	ui->write(ui->controller, ZAMCOMP_RATIO, sizeof(float), 0, (const void*) &ui->knobs[3]);
	ui->knobs[4] = robtk_spin_get_value(ui->knob_makeup);
	ui->write(ui->controller, ZAMCOMP_MAKEUP, sizeof(float), 0, (const void*) &ui->knobs[4]);
	ui->knobs[5] = robtk_scale_get_value(ui->slider_thresh);
	ui->write(ui->controller, ZAMCOMP_THRESH, sizeof(float), 0, (const void*) &ui->knobs[5]);

	calceqcurve(ui, ui->compx, ui->compy);
	robtk_xydraw_set_points(ui->xyp, COMPOINTS, ui->compx, ui->compy);
	return TRUE;
}

static void render_frontface(ZamComp_UI* ui) {

	cairo_t *cr;
	robtk_xydraw_set_surface(ui->xyp, NULL);
	ui->compcurve = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, PLOT_W, PLOT_H);
	cr = cairo_create (ui->compcurve);

	CairoSetSouerceRGBA(c_blk);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_rectangle (cr, 0, 0, PLOT_W, PLOT_H);
	cairo_fill (cr);

	cairo_save(cr);
	rounded_rectangle (cr, 10, 10, PLOT_W - 20, PLOT_H - 20, 10);
	CairoSetSouerceRGBA(c_blk);
	cairo_fill_preserve(cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	
	robtk_xydraw_set_surface(ui->xyp, ui->compcurve);
	
	calceqcurve(ui, ui->compx, ui->compy);
	robtk_xydraw_set_points(ui->xyp, COMPOINTS, ui->compx, ui->compy);

	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.8);
	const double dash[] = {1.5};
	cairo_set_line_width(cr, 1.5);
	cairo_set_dash(cr, dash, 1, 0);
	//cairo_move_to(cr, 0, PLOT_H/2.);
	//cairo_line_to(cr, PLOT_W, PLOT_H/2.);
	cairo_stroke(cr);
	// XXX Doesn't print graph since knob values are NaN.. why?
}

static void ui_disable(LV2UI_Handle handle)
{
}

static void ui_enable(LV2UI_Handle handle)
{
}

static RobWidget * toplevel(ZamComp_UI* ui, void * const top)
{

        ui->frontface = NULL;
	ui->hbox = rob_hbox_new(FALSE, 2);
        robwidget_make_toplevel(ui->hbox, top);
        ROBWIDGET_SETNAME(ui->hbox, "ZamComp");

        ui->ctable = rob_table_new(/*rows*/6, /*cols*/ 5, FALSE);
	ui->sep[0] = robtk_sep_new(TRUE);
	ui->sep[1] = robtk_sep_new(TRUE);
	ui->sep[2] = robtk_sep_new(TRUE);

	ui->knob_att      = robtk_spin_new(0.2, 80, 0.1);
	ui->knob_rel      = robtk_spin_new(2, 500, 1);
	ui->knob_knee     = robtk_spin_new(0, 9, 0.1);
	ui->knob_ratio    = robtk_spin_new(1.0, 20, 0.1);
	ui->knob_makeup   = robtk_spin_new(0, 30, 0.5);
	ui->slider_thresh = robtk_scale_new(-60, 0, 0.5, false);

	ui->lbl_att    = robtk_lbl_new("ATT");
	ui->lbl_rel    = robtk_lbl_new("REL");
	ui->lbl_knee   = robtk_lbl_new("KN");
	ui->lbl_ratio  = robtk_lbl_new("RAT");
	ui->lbl_makeup = robtk_lbl_new("MAK");
	ui->lbl_thresh = robtk_lbl_new("THR");

	//ui->darea = robtk_darea_new(PLOT_W,PLOT_H, &expose_plot, ui);

	ui->xyp = robtk_xydraw_new(PLOT_W, PLOT_H);
	robtk_xydraw_set_alignment(ui->xyp, 0, 0);
	robtk_xydraw_set_linewidth(ui->xyp, 2.5);
	robtk_xydraw_set_drawing_mode(ui->xyp, RobTkXY_yraw_line);
	robtk_xydraw_set_mapping(ui->xyp, 1., 0., 1., 0.);
	robtk_xydraw_set_area(ui->xyp, 10, 10, PLOT_W-20, PLOT_H-20);
	robtk_xydraw_set_clip_callback(ui->xyp, xy_clip_fn, ui);
	robtk_xydraw_set_color(ui->xyp, 1.0, .2, .0, 1.0);

	ui->logo = robtk_img_new(LOGO_W, LOGO_H, 4, img_logo.pixel_data);
	robtk_img_set_alignment(ui->logo, 0, 0);

	int row = 0;

	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_thresh),
		0, 1, row+4, row+5, 0, 0, RTK_EXANDF, RTK_SHRINK);
	rob_table_attach(ui->ctable, robtk_scale_widget(ui->slider_thresh),
		0, 1, row, row+4, 0, 0, RTK_EXANDF, RTK_SHRINK);

	rob_table_attach(ui->ctable, robtk_spin_widget(ui->knob_att),
		1, 2, row, row+1, 0, 0, RTK_EXANDF, RTK_SHRINK);
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_att),
		2, 3, row, row+1, 0, 0, RTK_EXANDF, RTK_SHRINK);
	rob_table_attach(ui->ctable, robtk_spin_widget(ui->knob_rel),
		3, 4, row, row+1, 0, 0, RTK_EXANDF, RTK_SHRINK);
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_rel),
		4, 5, row, row+1, 0, 0, RTK_EXANDF, RTK_SHRINK);
	row++;
	
	rob_table_attach(ui->ctable, robtk_spin_widget(ui->knob_knee),
		1, 2, row, row+1, 0, 0, RTK_EXANDF, RTK_SHRINK);
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_knee),
		2, 3, row, row+1, 0, 0, RTK_EXANDF, RTK_SHRINK);
	rob_table_attach(ui->ctable, robtk_spin_widget(ui->knob_ratio),
		3, 4, row, row+1, 0, 0, RTK_EXANDF, RTK_SHRINK);
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_ratio),
		4, 5, row, row+1, 0, 0, RTK_EXANDF, RTK_SHRINK);
	row++;

	rob_table_attach(ui->ctable, robtk_spin_widget(ui->knob_makeup),
		1, 2, row, row+1, 0, 0, RTK_EXANDF, RTK_SHRINK);
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_makeup),
		2, 5, row, row+1, 0, 0, RTK_EXANDF, RTK_SHRINK);
	row++;

	rob_table_attach(ui->ctable, robtk_img_widget(ui->logo),
		1, 5, row, row+1, 0, 0, RTK_EXANDF, RTK_SHRINK);

#define SPIN_DFTNVAL(SPB, VAL) \
	robtk_spin_set_default(SPB, VAL); \
	robtk_spin_set_value(SPB, VAL);

	robtk_spin_set_default(ui->knob_att, 10.);
	robtk_spin_set_default(ui->knob_rel, 80.);
	robtk_spin_set_default(ui->knob_knee, 3.);
	robtk_spin_set_default(ui->knob_ratio, 3.);
	robtk_spin_set_default(ui->knob_makeup, 0.);
	robtk_scale_set_default(ui->slider_thresh, 0.);

      	robtk_spin_set_callback(ui->knob_att, cb_set_knobs, ui);
      	robtk_spin_set_callback(ui->knob_rel, cb_set_knobs, ui);
      	robtk_spin_set_callback(ui->knob_knee, cb_set_knobs, ui);
	robtk_spin_set_callback(ui->knob_ratio, cb_set_knobs, ui);
	robtk_spin_set_callback(ui->knob_makeup, cb_set_knobs, ui);
	robtk_scale_set_callback(ui->slider_thresh, cb_set_knobs, ui);

	rob_hbox_child_pack(ui->hbox, robtk_xydraw_widget(ui->xyp), FALSE, FALSE);
        rob_hbox_child_pack(ui->hbox, ui->ctable, FALSE, FALSE);
	
	return ui->hbox;
}

/******************************************************************************
 * LV2
 */

static LV2UI_Handle
instantiate(
		void* const               ui_toplevel,
		const LV2UI_Descriptor*   descriptor,
		const char*               plugin_uri,
		const char*               bundle_path,
		LV2UI_Write_Function      write_function,
		LV2UI_Controller          controller,
		RobWidget**               widget,
		const LV2_Feature* const* features)
{
	ZamComp_UI* ui = (ZamComp_UI*)calloc(1, sizeof(ZamComp_UI));

	if (!ui) {
		fprintf(stderr, "ZamComp UI: out of memory\n");
		return NULL;
	}

	*widget = NULL;
	ui->first = true;

	for (uint32_t i = 0; i < 6; ++i) {
		ui->knobs[i] = 0.;
	}

	/* initialize private data structure */
	ui->write      = write_function;
	ui->controller = controller;

	*widget = toplevel(ui, ui_toplevel);
	//printf("1\n");
	render_frontface(ui);
	ui->first = false;
	return ui;
}

static enum LVGLResize
plugin_scale_mode(LV2UI_Handle handle)
{
	return LVGL_LAYOUT_TO_FIT;
}

static void
cleanup(LV2UI_Handle handle)
{
	ZamComp_UI* ui = (ZamComp_UI*)handle;
	ui_disable(ui);

	robtk_xydraw_set_surface(ui->xyp, NULL);
	cairo_surface_destroy (ui->compcurve);
	robtk_xydraw_destroy(ui->xyp);
	
	robtk_lbl_destroy(ui->lbl_att);
	robtk_lbl_destroy(ui->lbl_rel);
	robtk_lbl_destroy(ui->lbl_knee);
	robtk_lbl_destroy(ui->lbl_ratio);
	robtk_lbl_destroy(ui->lbl_makeup);
	robtk_lbl_destroy(ui->lbl_thresh);
	robtk_spin_destroy(ui->knob_att);
	robtk_spin_destroy(ui->knob_rel);
	robtk_spin_destroy(ui->knob_knee);
	robtk_spin_destroy(ui->knob_ratio);
	robtk_spin_destroy(ui->knob_makeup);
	robtk_scale_destroy(ui->slider_thresh);

	robtk_img_destroy(ui->logo);

	rob_box_destroy(ui->hbox);
	cairo_surface_destroy(ui->frontface);

	free(ui);
}

static void
port_event(LV2UI_Handle handle,
		uint32_t     port_index,
		uint32_t     buffer_size,
		uint32_t     format,
		const void*  buffer)
{
	ZamComp_UI* ui = (ZamComp_UI*)handle;
	

	if (format != 0) return;
	const float v = *(float *)buffer;
	switch (port_index) {
		case ZAMCOMP_ATTACK:
			ui->disable_signals = true;
			robtk_spin_set_value(ui->knob_att, v);
			ui->knobs[0] = v;
			ui->disable_signals = false;
			break;
		case ZAMCOMP_RELEASE:
			ui->disable_signals = true;
			robtk_spin_set_value(ui->knob_rel, v);
			ui->knobs[1] = v;
			ui->disable_signals = false;
			break;
		case ZAMCOMP_KNEE:
			ui->disable_signals = true;
			robtk_spin_set_value(ui->knob_knee, v);
			ui->knobs[2] = v;
			ui->disable_signals = false;
			break;
		case ZAMCOMP_RATIO:
			ui->disable_signals = true;
			robtk_spin_set_value(ui->knob_ratio, v);
			ui->knobs[3] = v;
			ui->disable_signals = false;
			break;
		case ZAMCOMP_MAKEUP:
			ui->disable_signals = true;
			robtk_spin_set_value(ui->knob_makeup, v);
			ui->knobs[4] = v;
			ui->disable_signals = false;
			break;
		case ZAMCOMP_THRESH:
			ui->disable_signals = true;
			robtk_scale_set_value(ui->slider_thresh, v);
			ui->knobs[5] = v;
			ui->disable_signals = false;
			break;
		default:
			return;
	}
}

static const void*
extension_data(const char* uri)
{
	return NULL;
}

