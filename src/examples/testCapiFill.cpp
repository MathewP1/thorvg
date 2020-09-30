#include <Elementary.h>
#include <thorvg_capi.h>
#include <vector>

#define WIDTH 800
#define HEIGHT 800
#define MARGIN 20
#define RECT_SIZE 80


/************************************************************************/
/* Capi Test Code                                                       */
/************************************************************************/

static uint32_t buffer[WIDTH * HEIGHT];
Tvg_Canvas* canvas;
int current_screen = 1;
std::vector<Tvg_Paint*> shapes;
Eo* view;

void reset()
{
    tvg_canvas_destroy(canvas);
    canvas = nullptr;
    shapes.clear();
    canvas = tvg_swcanvas_create();
    tvg_swcanvas_set_target(canvas, buffer, WIDTH, WIDTH, HEIGHT, TVG_COLORSPACE_ARGB8888);
}

void test2()
{
    reset();

    Tvg_Paint* shape;
    int rect_cnt = WIDTH / (MARGIN + RECT_SIZE);
    for (int i = 0; i < rect_cnt; i++){
        for (int j = 0; j < rect_cnt; j++){
            shape = tvg_shape_new();
            tvg_shape_append_rect(shape, i * (MARGIN+RECT_SIZE), j * (MARGIN+RECT_SIZE), RECT_SIZE, RECT_SIZE, 0, 0);
            shapes.push_back(shape);
        }
    }

    for (auto shape : shapes){
        tvg_canvas_push(canvas, shape);
        tvg_shape_set_fill_color(shape, 0, 255, 0, 255);
        tvg_canvas_update_paint(canvas, shape);
    }

    tvg_canvas_draw(canvas);
    tvg_canvas_sync(canvas);

    evas_object_image_data_set(view, buffer);
    evas_object_image_data_update_add(view, 0, 0, WIDTH, HEIGHT);
}


void test1()
{
    reset();

    Tvg_Paint* shape;
    shape = tvg_shape_new();
    tvg_shape_append_circle(shape, 400, 400, 200, 200);
    tvg_shape_set_fill_color(shape, 255, 0, 0, 255);
    shapes.push_back(shape);

    tvg_canvas_push(canvas, shape);
    tvg_canvas_update_paint(canvas, shape);

    tvg_canvas_draw(canvas);
    tvg_canvas_sync(canvas);

    evas_object_image_data_set(view, buffer);
    evas_object_image_data_update_add(view, 0, 0, WIDTH, HEIGHT);

}

/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

static void
_on_keydown(void *data EINA_UNUSED,
            Evas *evas EINA_UNUSED,
            Evas_Object *o EINA_UNUSED,
            void *event_info)
{
   Evas_Event_Key_Down *ei = static_cast<Evas_Event_Key_Down*>(event_info);

   if (!strcmp(ei->key, "1") && current_screen != 1){
       printf("1\n");
      test1();
      current_screen = 1;
   }
   if (!strcmp(ei->key, "2") && current_screen != 2){
       printf("2\n");
       test2();
       current_screen = 2;
   }
}

void win_del(void *data, Evas_Object *o, void *ev)
{
   elm_exit();
}


int main(int argc, char **argv)
{
    elm_init(argc, argv);

    Eo* win = elm_win_util_standard_add(NULL, "ThorVG Test");
    evas_object_smart_callback_add(win, "delete,request", win_del, 0);

    view = evas_object_image_filled_add(evas_object_evas_get(win));
    evas_object_image_size_set(view, WIDTH, HEIGHT);
    evas_object_image_data_set(view, buffer);
    evas_object_image_pixels_dirty_set(view, EINA_TRUE);
    evas_object_image_data_update_add(view, 0, 0, WIDTH, HEIGHT);
    evas_object_size_hint_weight_set(view, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_event_callback_add(win, EVAS_CALLBACK_KEY_DOWN, _on_keydown, NULL);
    evas_object_show(view);

    elm_win_resize_object_add(win, view);
    evas_object_geometry_set(win, 0, 0, WIDTH, HEIGHT);
    evas_object_show(win);

    // init thorVG
    tvg_engine_init(TVG_ENGINE_SW | TVG_ENGINE_GL, 0);

    test1();

    elm_run();
    elm_shutdown();

    tvg_canvas_destroy(canvas);
    tvg_engine_term(TVG_ENGINE_SW | TVG_ENGINE_GL);


    return 0;
}
