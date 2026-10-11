#include "menu_wifi.h"

#include <stdio.h>
#include <string.h>

#include "circle.h"
#include "menu.h"

static struct menu_item *wifi_ssid_item;
static struct menu_item *wifi_security_item;
static struct wifi_access_point wifi_access_points[MAX_WIFI_ACCESS_POINTS];
static int wifi_access_point_count;

static void sort_wifi_access_points(void) {
  for (int index = 1; index < wifi_access_point_count; index++) {
    struct wifi_access_point access_point = wifi_access_points[index];
    int sorted_index = index;
    while (sorted_index > 0 &&
           wifi_access_points[sorted_index - 1].signal < access_point.signal) {
      wifi_access_points[sorted_index] =
          wifi_access_points[sorted_index - 1];
      sorted_index--;
    }
    wifi_access_points[sorted_index] = access_point;
  }
}

static void wifi_access_point_selected(struct menu_item *item) {
  if (item->value < 0 || item->value >= wifi_access_point_count) {
    return;
  }

  strncpy(wifi_ssid_item->str_value, wifi_access_points[item->value].ssid,
          wifi_ssid_item->max_length);
  wifi_ssid_item->str_value[wifi_ssid_item->max_length] = '\0';
  wifi_ssid_item->value = strlen(wifi_ssid_item->str_value);
  wifi_security_item->value = wifi_access_points[item->value].secure ? 0 : 1;
  ui_pop_menu();
}




static void aggiungi_a_righe(struct menu_item *root, const char *testo) {
  char riga[MAX_MENU_STR];
  size_t resto = strlen(testo);
  while (resto > 0) {
    size_t n = resto;
    if (n > MAX_MENU_STR - 1) {
      n = MAX_MENU_STR - 1;
      for (size_t j = n; j > MAX_MENU_STR / 2; j--) {
        if (strchr(".,-", testo[j - 1]) != NULL) {
          n = j;
          break;
        }
      }
    }
    memcpy(riga, testo, n);
    riga[n] = '\0';
    ui_menu_add_button(MENU_ID_DO_NOTHING, root, riga)->disabled = 1;
    testo += n;
    resto -= n;
  }
}

void show_wifi_access_points(struct menu_item *ssid_item,
                             struct menu_item *security_item) {
  wifi_access_point_count = circle_scan_wifi_access_points(
      wifi_access_points, MAX_WIFI_ACCESS_POINTS);
  ui_pop_menu();
  struct menu_item *root = ui_push_menu(42, 14);

  wifi_ssid_item = ssid_item;
  wifi_security_item = security_item;
  if (wifi_access_point_count == CIRCLE_WIFI_SCAN_NO_FIRMWARE) {


    const char *manca = circle_missing_wifi_firmware();
    ui_menu_add_button(MENU_ID_DO_NOTHING, root,
                       "Missing in the firmware folder:")->disabled = 1;
    if (manca != NULL) {
      aggiungi_a_righe(root, manca);
    }
    ui_menu_add_button(MENU_ID_DO_NOTHING, root,
                       "Copy it from the whole-card .7z")->disabled = 1;
    return;
  }
  if (wifi_access_point_count == CIRCLE_WIFI_SCAN_NOT_STARTED) {
    ui_menu_add_button(MENU_ID_DO_NOTHING, root,
                       "The WiFi chip did not start")->disabled = 1;
    return;
  }
  if (wifi_access_point_count <= 0) {
    ui_menu_add_button(MENU_ID_DO_NOTHING, root, "No WiFi networks found")->disabled = 1;
    return;
  }

  sort_wifi_access_points();

  ui_menu_add_button(MENU_ID_DO_NOTHING, root,
                     "SSID                  MHz  CH RSSI")->disabled = 1;
  ui_menu_add_divider(root);
  for (int index = 0; index < wifi_access_point_count; index++) {
    char row[64];
    snprintf(row, sizeof(row), "%-20.20s %4d %3d %4d",
             wifi_access_points[index].ssid, wifi_access_points[index].mhz,
             wifi_access_points[index].channel,
             wifi_access_points[index].signal);
    struct menu_item *item = ui_menu_add_button_with_value(
        MENU_WIFI_SSID, root, row, index, "", "");
    item->on_value_changed = wifi_access_point_selected;
  }
}