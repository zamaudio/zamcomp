#include <gtkmm.h>
#include <lv2gui.hpp>
#include "zamcomp.peg"


using namespace sigc;
using namespace Gtk;


class ZamCompGUI : public LV2::GUI<ZamCompGUI> {
public:

  ZamCompGUI(const std::string& URI) {
    Table* table = manage(new Table(2, 2));
    table->attach(*manage(new Label("Attack")), 0, 1, 0, 1);
    table->attach(*manage(new Label("Release")), 0, 1, 1, 2);
    w_scale = manage(new HScale(p_ports[p_attack].min,
				p_ports[p_attack].max, 0.01));
    b_scale = manage(new HScale(p_ports[p_release].min,
				p_ports[p_release].max, 0.01));
    w_scale->set_size_request(100, -1);
    b_scale->set_size_request(100, -1);
    slot<void> w_slot = compose(bind<0>(mem_fun(*this, &ZamCompGUI::write_control), p_attack),
				mem_fun(*w_scale, &HScale::get_value));
    slot<void> b_slot = compose(bind<0>(mem_fun(*this, &ZamCompGUI::write_control), p_release),
				mem_fun(*b_scale, &HScale::get_value));
    w_scale->signal_value_changed().connect(w_slot);
    b_scale->signal_value_changed().connect(b_slot);
    table->attach(*w_scale, 1, 2, 0, 1);
    table->attach(*b_scale, 1, 2, 1, 2);
    add(*table);
  }
  
  void port_event(uint32_t port, uint32_t buffer_size, uint32_t format, const void* buffer) {
    if (port == p_attack)
      w_scale->set_value(*static_cast<const float*>(buffer));
    else if (port == p_release)
      b_scale->set_value(*static_cast<const float*>(buffer));
  }

protected:
  
  HScale* w_scale;
  HScale* b_scale;
  
};


static int _ = ZamCompGUI::register_class("http://zamaudio.com/lv2/zamcomp/gui");
