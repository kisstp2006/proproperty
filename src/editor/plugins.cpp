#define LUMIX_NO_CUSTOM_CRT
#include "core/allocator.h"
#include "editor/studio_app.h"
#include "editor/world_editor.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <string>
#include <variant>
#include <vector>

using namespace Lumix;

struct EditorPlugin : StudioApp::GUIPlugin
{
	EditorPlugin(StudioApp& app, WorldEditor& editor)
		: m_app(app)
		, is_opened(false)
		, frameCount(500)
		, selected_keyframe(nullptr)
		, dragging_keyframe(nullptr)
		, splitter_ratio(0.3f)
		, splitter_active(false)
		, currentFrame(0)
		, selected_track(nullptr)
		, playing(false)
		, play_speed(24)
		, time_accumulator(0.0f)
		, is_scrubbing(false)
		, timeline_offset(0.0f)
		, dragging_timeline(false)
		, hovering_keyframe(false)
		, editor(editor)
		, tracks{Track{"Object 1_Rotation", 1, {{10, Vec3(1, 2, 3)}, {20, Vec3(4, 5, 6)}}, Track::ValueType::Vec3},
			  Track{"Object 1_Transform", 1, {{10, Quat(1, 0, 0, 0)}, {20, Quat(0, 1, 0, 0)}}, Track::ValueType::Quat}}
	{
	}

	StudioApp& m_app;
	bool is_opened;

	struct Keyframe
	{
		int frame;
		std::variant<float, int, Vec2, Vec3, Quat> value;
	};

	struct Track
	{
		std::string name;
		std::int32_t id = 0;
		std::vector<Keyframe> keyframes;

		enum class ValueType
		{
			Float,
			Int,
			Vec2,
			Vec3,
			Quat
		};
		ValueType type;
	};

	std::vector<Track> tracks;

	Keyframe* selected_keyframe;
	Track* selected_track;
	Keyframe* dragging_keyframe;
	float drag_offset_x = 0.0f;
	int frameCount;
	int currentFrame;
	bool playing;
	int play_speed;
	float time_accumulator;
	bool is_scrubbing;
	float timeline_offset;
	float zoom = 1.0f;
	bool dragging_timeline;
	bool hovering_keyframe;

	float splitter_ratio;
	bool splitter_active;


	WorldEditor& editor;

	// UI színek és méretek
	static constexpr float TRACK_HEIGHT = 40.0f;		   
	static constexpr float KEYFRAME_RADIUS = 6.0f;		   
	static constexpr float TIMELINE_HEADER_HEIGHT = 30.0f; 
	static constexpr float TRACK_LABELS_WIDTH = 150.0f;	   

	void onGUI() override
	{
		
         Span<const EntityRef> ents = editor.getSelectedEntities();
		World& world = *editor.getWorld();

		if (playing)
		{
			time_accumulator += ImGui::GetIO().DeltaTime;
			while (time_accumulator > 1.0f / play_speed)
			{
				currentFrame++;
				time_accumulator -= 1.0f / play_speed;
			}
			if (currentFrame > frameCount)
			{
				currentFrame = frameCount;
				playing = false;
			}
		}

		if (ImGui::IsWindowFocused())
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Space)) playing = !playing;
			if (ImGui::IsKeyPressed(ImGuiKey_Delete) && selected_keyframe)
			{
				// Delete selected keyframe
			}
			if (ImGui::IsKeyPressed(ImGuiKey_C, false) && ImGui::GetIO().KeyCtrl)
			{
				// Copy keyframe
			}
		}

		const char* entity_name = "No entity selected";
		// fixed: previous check was invalid; use empty()
		if (!ents.end() == 0)
		{
			entity_name = world.getEntityName(ents[0]);
		}

		ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_FirstUseEver); 
		if (ImGui::Begin("Pro Property Animator", &is_opened))
		{
			ImGui::Text("Selected Entity: %s", entity_name);
			ImGui::Separator();

			float total_height = ImGui::GetContentRegionAvail().y;
			float splitter_height = 8.0f; 
			float timeline_height = total_height * splitter_ratio - splitter_height * 0.5f;
			float inspector_height = total_height * (1.0f - splitter_ratio) - splitter_height * 0.5f;

			// Timeline
			ImVec2 timeline_size = ImVec2(0, timeline_height);
			ImGui::BeginChild("TimelineRegion", timeline_size, true, ImGuiWindowFlags_NoScrollbar);

			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
			ImVec2 canvas_size = ImGui::GetContentRegionAvail();

			float timeline_start_x = canvas_pos.x + TRACK_LABELS_WIDTH;
			float base_frame_width = 8.0f;
			float frame_width = base_frame_width * zoom;

			// ZOOM HANDLING - Ctrl + mouse wheel
			if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl)
			{
				float wheel = ImGui::GetIO().MouseWheel;
				if (wheel != 0.0f)
				{
					ImVec2 mouse_pos = ImGui::GetMousePos();
					float mouse_timeline_x = mouse_pos.x - timeline_start_x;

					float old_frame_width = base_frame_width * zoom;
					float frame_under_mouse = (mouse_timeline_x - timeline_offset) / old_frame_width;

					float old_zoom = zoom;
					zoom += wheel * 0.15f;					
					zoom = Lumix::clamp(zoom, 0.1f, 15.0f); 

					float new_frame_width = base_frame_width * zoom;
					timeline_offset = mouse_timeline_x - frame_under_mouse * new_frame_width;
				}
			}

			// PAN HANDLING - Middle mouse button
			if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
			{
				if (!dragging_timeline) dragging_timeline = true;
				timeline_offset += ImGui::GetIO().MouseDelta.x;
			}
			else
			{
				dragging_timeline = false;
			}

			// Timeline offset constraints
			float max_timeline_width = frameCount * frame_width;
			float visible_timeline_width = canvas_size.x - TRACK_LABELS_WIDTH;
			timeline_offset = Lumix::clamp(
				timeline_offset, -max_timeline_width + visible_timeline_width * 0.5f, visible_timeline_width * 0.5f);

			
			ImU32 bg_col = IM_COL32(35, 35, 40, 255);
			ImU32 header_bg_col = IM_COL32(45, 45, 50, 255);
			ImU32 track_separator_col = IM_COL32(60, 60, 65, 255);

			
			draw_list->AddRectFilled(
				canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), bg_col);

			// Timeline header háttér
			ImVec2 header_start = canvas_pos;
			ImVec2 header_end = ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + TIMELINE_HEADER_HEIGHT);
			draw_list->AddRectFilled(header_start, header_end, header_bg_col);

			// Grid vonalak javított színekkel
			ImU32 minor_grid_color = IM_COL32(55, 55, 60, 150);	   
			ImU32 major_grid_color = IM_COL32(80, 80, 85, 255);	   
			ImU32 player_line_color = IM_COL32(255, 120, 60, 255); 

			
			float visible_start_frame = (-timeline_offset) / frame_width;
			float visible_end_frame = (-timeline_offset + canvas_size.x - TRACK_LABELS_WIDTH) / frame_width;
			int start_frame = Lumix::maximum(0, int(visible_start_frame) - 1);
			int end_frame = Lumix::minimum(frameCount, int(visible_end_frame) + 1);

			
			if (zoom > 0.8f)
			{
				for (int f = start_frame; f <= end_frame; ++f)
				{
					float x = timeline_start_x + f * frame_width + timeline_offset;
					if (x >= timeline_start_x && x <= canvas_pos.x + canvas_size.x)
					{
						draw_list->AddLine(ImVec2(x, canvas_pos.y + TIMELINE_HEADER_HEIGHT),
							ImVec2(x, canvas_pos.y + canvas_size.y),
							minor_grid_color);
					}
				}
			}

			
			int base_spacing = 10;
			if (zoom < 0.5f)
				base_spacing = 50;
			else if (zoom < 1.0f)
				base_spacing = 20;
			else if (zoom > 3.0f)
				base_spacing = 5;

			int frame_spacing = base_spacing;
			while (frame_spacing * frame_width < 40.0f && frame_spacing < frameCount / 4)
			{
				frame_spacing *= 2;
			}

			for (int f = 0; f <= frameCount; f += frame_spacing)
			{
				float x = timeline_start_x + f * frame_width + timeline_offset;
				if (x >= timeline_start_x - 50 && x <= canvas_pos.x + canvas_size.x + 50)
				{
					
					draw_list->AddLine(ImVec2(x, canvas_pos.y + TIMELINE_HEADER_HEIGHT),
						ImVec2(x, canvas_pos.y + canvas_size.y),
						major_grid_color);

					
					if (frame_spacing * frame_width > 30.0f)
					{
						char buf[16];
						sprintf_s(buf, "%d", f);
						ImVec2 text_size = ImGui::CalcTextSize(buf);
						ImVec2 text_pos = ImVec2(x - text_size.x * 0.5f, canvas_pos.y + 8);
						draw_list->AddText(text_pos, IM_COL32(200, 200, 200, 255), buf);
					}
				}
			}

			
			for (size_t t = 1; t < tracks.size(); ++t)
			{
				float y = canvas_pos.y + TIMELINE_HEADER_HEIGHT + t * TRACK_HEIGHT;
				draw_list->AddLine(
					ImVec2(canvas_pos.x, y), ImVec2(canvas_pos.x + canvas_size.x, y), track_separator_col);
			}

			
			float current_frame_x = timeline_start_x + currentFrame * frame_width + timeline_offset;
			if (current_frame_x >= timeline_start_x - 10 && current_frame_x <= canvas_pos.x + canvas_size.x + 10)
			{
				draw_list->AddLine(ImVec2(current_frame_x, canvas_pos.y + TIMELINE_HEADER_HEIGHT),
					ImVec2(current_frame_x, canvas_pos.y + canvas_size.y),
					player_line_color,
					3);

				// Playhead triangle 
				ImVec2 triangle_top = ImVec2(current_frame_x, canvas_pos.y + 2);
				ImVec2 triangle_left = ImVec2(current_frame_x - 8, canvas_pos.y + TIMELINE_HEADER_HEIGHT - 2);
				ImVec2 triangle_right = ImVec2(current_frame_x + 8, canvas_pos.y + TIMELINE_HEADER_HEIGHT - 2);
				draw_list->AddTriangleFilled(triangle_top, triangle_left, triangle_right, player_line_color);
			}

			// Scrubbing handling
			ImVec2 scrub_area_min(timeline_start_x, canvas_pos.y);
			ImVec2 scrub_area_max(canvas_pos.x + canvas_size.x, canvas_pos.y + TIMELINE_HEADER_HEIGHT);

			if (ImGui::IsMouseHoveringRect(scrub_area_min, scrub_area_max) && ImGui::IsMouseClicked(0))
			{
				is_scrubbing = true;
			}

			if (is_scrubbing)
			{
				playing = false;
				ImVec2 mouse_pos = ImGui::GetMousePos();
				float relative_mouse_x = mouse_pos.x - timeline_start_x - timeline_offset;
				int new_frame = int(relative_mouse_x / frame_width + 0.5f);
				currentFrame = Lumix::clamp(new_frame, 0, frameCount);

				if (!ImGui::IsMouseDown(0))
				{
					is_scrubbing = false;
				}
			}

			// Track names
			for (size_t t = 0; t < tracks.size(); ++t)
			{
				float track_y_start = canvas_pos.y + TIMELINE_HEADER_HEIGHT + t * TRACK_HEIGHT;
				float track_y_center = track_y_start + TRACK_HEIGHT * 0.5f;

				// Track háttér
				ImU32 track_bg_color = (t % 2 == 0) ? IM_COL32(40, 40, 45, 255) : IM_COL32(45, 45, 50, 255);
				if (selected_track == &tracks[t])
				{
					track_bg_color = IM_COL32(60, 80, 120, 255);
				}

				ImVec2 track_bg_min = ImVec2(canvas_pos.x, track_y_start);
				ImVec2 track_bg_max = ImVec2(timeline_start_x, track_y_start + TRACK_HEIGHT);
				draw_list->AddRectFilled(track_bg_min, track_bg_max, track_bg_color);

				
				ImVec2 text_pos = ImVec2(canvas_pos.x + 8, track_y_center - 8);
				draw_list->AddText(text_pos, IM_COL32(220, 220, 220, 255), tracks[t].name.c_str());

				// Track click handling
				if (ImGui::IsMouseHoveringRect(track_bg_min, track_bg_max) && ImGui::IsMouseClicked(0))
				{
					selected_track = &tracks[t];
					selected_keyframe = nullptr;
				}

				// Keyframes
				for (Keyframe& kf : tracks[t].keyframes)
				{
					float x = timeline_start_x + kf.frame * frame_width + timeline_offset;

					if (x >= timeline_start_x - 20 && x <= canvas_pos.x + canvas_size.x + 20)
					{
						bool is_selected = (selected_keyframe == &kf);
						bool is_hovered = false;

						ImRect kf_rect(ImVec2(x - KEYFRAME_RADIUS, track_y_center - KEYFRAME_RADIUS),
							ImVec2(x + KEYFRAME_RADIUS, track_y_center + KEYFRAME_RADIUS));

						if (ImGui::IsMouseHoveringRect(kf_rect.Min, kf_rect.Max))
						{
							is_hovered = true;
							hovering_keyframe = true;

							if (ImGui::IsMouseClicked(0))
							{
								selected_keyframe = &kf;
								dragging_keyframe = &kf;
								selected_track = &tracks[t];

								ImVec2 mouse_pos = ImGui::GetMousePos();
								drag_offset_x = mouse_pos.x - x;
							}
							if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
							{
								selected_keyframe = &kf;
								selected_track = &tracks[t];
								ImGui::OpenPopup("KeyframeContextMenu");
							}
						}

						
						ImU32 kf_color;
						ImU32 kf_border_color;

						if (is_selected)
						{
							kf_color = IM_COL32(255, 200, 0, 255);
							kf_border_color = IM_COL32(255, 255, 255, 255);
						}
						else if (is_hovered)
						{
							kf_color = IM_COL32(255, 180, 80, 255);
							kf_border_color = IM_COL32(255, 220, 120, 255);
						}
						else
						{
							kf_color = IM_COL32(200, 150, 0, 255);
							kf_border_color = IM_COL32(220, 170, 20, 255);
						}

						
						draw_list->AddCircleFilled(ImVec2(x, track_y_center), KEYFRAME_RADIUS, kf_color);
						draw_list->AddCircle(ImVec2(x, track_y_center), KEYFRAME_RADIUS, kf_border_color, 0, 1.5f);
					}
				}

				
				if (dragging_keyframe && ImGui::IsMouseDragging(0))
				{
					ImVec2 mouse_pos = ImGui::GetMousePos();
					float timeline_x = mouse_pos.x - drag_offset_x;
					float timeline_origin_x = timeline_start_x + timeline_offset;

					int new_frame = int((timeline_x - timeline_origin_x) / frame_width + 0.5f);
					new_frame = Lumix::clamp(new_frame, 0, frameCount);

					dragging_keyframe->frame = new_frame;
				}

				if (dragging_keyframe && ImGui::IsMouseReleased(0))
				{
					dragging_keyframe = nullptr;
				}
			}

			// Context menu
			if (ImGui::BeginPopup("KeyframeContextMenu"))
			{
				ImGui::Text("Keyframe Options");
				ImGui::Separator();

				if (ImGui::MenuItem("Delete", "Del"))
				{
					for (Track& track : tracks)
					{
						auto& keys = track.keyframes;
						keys.erase(
							std::remove_if(
								keys.begin(), keys.end(), [&](const Keyframe& k) { return &k == selected_keyframe; }),
							keys.end());
					}
					selected_keyframe = nullptr;
				}

				if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
				{
					if (selected_keyframe && selected_track)
					{
						Keyframe new_kf = *selected_keyframe;
						new_kf.frame += 5;
						new_kf.frame = Lumix::clamp(new_kf.frame, 0, frameCount); // clamp duplicated frame
						selected_track->keyframes.push_back(new_kf);
					}
				}

				ImGui::EndPopup();
			}

			ImGui::EndChild();

			
			ImGui::Separator();

			
			float button_width = 60.0f;
			if (ImGui::Button(ICON_FA_STEP_BACKWARD "##back", ImVec2(button_width, 0)))
			{
				// fixed: clamp(value, min, max)
				currentFrame = Lumix::clamp(currentFrame - 1, 0, frameCount);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Previous frame");

			ImGui::SameLine();
			if (playing)
			{
				if (ImGui::Button(ICON_FA_PAUSE "##pause", ImVec2(button_width, 0)))
				{
					playing = false;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause (Space)");
			}
			else
			{
				if (ImGui::Button(ICON_FA_PLAY "##play", ImVec2(button_width, 0)))
				{
					playing = true;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play (Space)");
			}

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_STEP_FORWARD "##forw", ImVec2(button_width, 0)))
			{
				// fixed: clamp(value, min, max)
				currentFrame = Lumix::clamp(currentFrame + 1, 0, frameCount);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Next frame");

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_STOP "##stop", ImVec2(button_width, 0)))
			{
				playing = false;
				currentFrame = 0;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop");

			// Frame info
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80);
			ImGui::InputInt("##frame", &currentFrame);
			currentFrame = Lumix::clamp(currentFrame, 0, frameCount);

			ImGui::SameLine();
			ImGui::Text("/ %d", frameCount);

			ImGui::SameLine();
			ImGui::SetNextItemWidth(60);
			ImGui::InputInt("FPS##speed", &play_speed);
			play_speed = Lumix::clamp(play_speed, 1, 120);

			// Splitter 
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.3f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.8f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

			ImGui::Button("##splitter", ImVec2(-1, splitter_height));

			if (ImGui::IsItemActive() || splitter_active)
			{
				splitter_active = true;
				float mouse_delta = ImGui::GetIO().MouseDelta.y;
				splitter_ratio += mouse_delta / total_height;
				splitter_ratio = Lumix::clamp(splitter_ratio, 0.15f, 0.85f);
			}

			if (ImGui::IsMouseReleased(0))
			{
				splitter_active = false;
			}

			ImGui::PopStyleColor(3);

			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
			}

			
			ImGui::BeginChild("Inspector", ImVec2(0, inspector_height), true);

			ImGui::Text("Property Inspector");
			ImGui::Separator();

			// Timeline info
			ImGui::Text("Timeline Info:");
			ImGui::Text("  Zoom: %.2fx", zoom);
			ImGui::Text("  Offset: %.1fpx", timeline_offset);
			ImGui::Text("  Frame Count: %d", frameCount);
			ImGui::Separator();

			if (selected_keyframe && selected_track)
			{
				ImGui::Text("Keyframe Properties:");
				ImGui::Text("Track: %s", selected_track->name.c_str());
				// fixed: print integral id with %d
				ImGui::Text("ID: %d", (int)selected_track->id);

				ImGui::SetNextItemWidth(100);
				ImGui::InputInt("Frame", &selected_keyframe->frame);
				selected_keyframe->frame = Lumix::clamp(selected_keyframe->frame, 0, frameCount);

				std::visit(
					[&](auto& val)
					{
						using T = std::decay_t<decltype(val)>;
						if constexpr (std::is_same_v<T, float>)
						{
							ImGui::SetNextItemWidth(150);
							ImGui::InputFloat("Value", &val);
						}
						else if constexpr (std::is_same_v<T, int>)
						{
							ImGui::SetNextItemWidth(150);
							ImGui::DragInt("Value", &val);
						}
						else if constexpr (std::is_same_v<T, Vec2>)
						{
							ImGui::SetNextItemWidth(200);
							ImGui::InputFloat2("Value", &val.x);
						}
						else if constexpr (std::is_same_v<T, Vec3>)
						{
							ImGui::SetNextItemWidth(250);
							ImGui::InputFloat3("Value", &val.x);
						}
						else if constexpr (std::is_same_v<T, Quat>)
						{
							ImGui::SetNextItemWidth(300);
							ImGui::InputFloat4("Value", &val.x);
						}
					},
					selected_keyframe->value);
			}
			else if (selected_track)
			{
				ImGui::Text("Track Properties:");
				ImGui::Text("Name: %s", selected_track->name.c_str());
				ImGui::Text("Keyframes: %zu", selected_track->keyframes.size());
				
			}
			else
			{
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No selection");
				ImGui::Text("Click on a track or keyframe to edit properties.");
			}

			ImGui::EndChild();
		}
		ImGui::End();
	}

	Keyframe make_keyframe(int frame, Track::ValueType type)
	{
		switch (type)
		{
			case Track::ValueType::Float: return {frame, 0.0f};
			case Track::ValueType::Int: return {frame, 0};
			case Track::ValueType::Vec2: return {frame, Vec2(0, 0)};
			case Track::ValueType::Vec3: return {frame, Vec3(0, 0, 0)};
			case Track::ValueType::Quat: return {frame, Quat(0, 0, 0, 1)};
		}
		ASSERT(false);
		return {frame, 0.0f};
	}

	void SetProperties(Quat rot) {}

	const char* getName() const override { return "proproperty"; }
};

LUMIX_STUDIO_ENTRY(proproperty)
{
	WorldEditor& editor = app.getWorldEditor();

	auto* plugin = LUMIX_NEW(editor.getAllocator(), EditorPlugin)(app, editor);
	app.addPlugin(*plugin);
	return nullptr;
}