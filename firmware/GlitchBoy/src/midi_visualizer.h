#ifndef MIDI_VISUALIZER_H
#define MIDI_VISUALIZER_H

#include <stddef.h>
#include <stdint.h>

void midi_visualizer_init(int width, int height, int flavor);

int midi_visualizer_mode_count();
void midi_visualizer_activate(int index);
void midi_visualizer_deactivate();
bool midi_visualizer_is_active();
void midi_visualizer_cycle_palette();
void midi_visualizer_cycle_sprite();
void midi_visualizer_set_sprite_control(uint8_t value);
void midi_visualizer_auto_advance_scene();

bool midi_visualizer_update(uint8_t** target_lines,
                            int target_width,
                            int target_height,
                            int target_pitch,
                            uint8_t* scratch,
                            size_t scratch_bytes);
#endif // MIDI_VISUALIZER_H
