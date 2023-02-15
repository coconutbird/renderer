#include <renderer/core.hpp>

#include <dwmapi.h>
#include <thread>

#include <glm/gtx/rotate_vector.hpp>
#include <fmt/core.h>

std::unique_ptr<renderer::win32_window> application;
std::unique_ptr<renderer::d3d11_renderer> dx11;

size_t segoe_font;

renderer::sync_manager updated_draw;
renderer::sync_manager updated_buf;

bool update_size = false;
bool close_requested = false;

int draw_count = 0;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CLOSE:
			close_requested = true;
			return 0;
		case WM_SIZE:
			application->set_size({LOWORD(lParam), HIWORD(lParam)});
			update_size = true;
			break;
		default:
			break;
	}

	return DefWindowProcA(hWnd, msg, wParam, lParam);
}

void draw_test_primitives(renderer::buffer* buf) {
	static renderer::timer rainbow_timer;
	static renderer::timer animation_timer;

	static float factor = 0.0f;
	static bool reverse = false;

	if (animation_timer.get_elapsed_duration() >= std::chrono::milliseconds(25)) {
		animation_timer.reset();

		if (reverse) {
			factor -= 0.02f;
			if (factor <= 0.0f) {
				reverse = false;
				factor = 0.0f;
			}
		}
		else {
			factor += 0.02f;
			if (factor >= 1.0f) {
				reverse = true;
				factor = 1.0f;
			}
		}
	}

	if (rainbow_timer.get_elapsed_duration() >= std::chrono::seconds(5)) {
		rainbow_timer.reset();
	}

	renderer::color_rgba rainbow = renderer::color_hsva(0.0f).ease(renderer::color_hsva(359.99f), static_cast<float>(rainbow_timer.get_elapsed_duration().count()) / 5000.0f);
	rainbow.a = 75;

	static std::vector<glm::vec2> points = {
		{400.0f, 500.0f},
		{700.0f, 500.0f},
		{600.0f, 350.0f},
		{700.0f, 300.0f},
		{500.0f, 200.0f},
		{500.0f, 600.0f},
		{600.0f, 600.0f}};

	// TODO: Polylines are broken
	static auto polyline = renderer::polyline_shape(points, rainbow, 20.0f, renderer::joint_miter);
	polyline.set_color(rainbow);

	const auto thickness = factor * 30.0f;
	const auto rounding = factor;
	const auto arc = factor * M_PI * 2;

	// Testing arc performance
	buf->draw_line({200.0f, 200.0f}, {300.0f, 300.0f}, COLOR_WHITE, thickness);
	buf->draw_rect({350.0f, 200.0f, 100.0f, 100.0f}, COLOR_RED, thickness);
	buf->draw_rect_filled({500.0f, 200.0f, 100.0f, 100.0f}, COLOR_ORANGE);
	buf->draw_rect_rounded({650.0f, 200.0f, 100.0f, 100.0f}, rounding, COLOR_YELLOW, thickness);
	buf->draw_rect_rounded_filled({800.0f, 200.0f, 100.0f, 100.0f}, factor, COLOR_GREEN);
	buf->draw_arc({250.0f, 400.0f}, arc, arc, 50.0f, COLOR_BLUE, thickness,
				  32, false);
	buf->draw_arc({400.0f, 400.0f}, arc, arc, 50.0f, COLOR_PURPLE, 0.0f, 32,
				  true);
	buf->draw_circle({550.0f, 400.0f}, 50.0f, COLOR_WHITE, thickness, 32);
	buf->draw_circle_filled({700.0f, 400.0f}, 50.0f, COLOR_RED, 32);

	buf->push_font(segoe_font);

	buf->draw_text<std::string>({25.0f, 60.0f}, "Hello, world!", COLOR_WHITE);
	buf->draw_text<std::u32string>({25.0f, 105.0f}, U"Unicode example: \u26F0", COLOR_WHITE);

	buf->pop_font();
}

void draw_thread() {
	const auto id = dx11->register_buffer();

	while (!close_requested) {
		updated_draw.wait();

		auto buf = dx11->get_working_buffer(id);

		draw_test_primitives(buf);

		dx11->swap_buffers(id);
		updated_buf.notify();
	}
}

// TODO: Mutex for texture creation and atlas
int main() {
	application = std::make_unique<renderer::win32_window>("D3D11 Renderer", glm::i32vec2{960, 500}, WndProc);

	if (!application->create()) {
		MessageBoxA(nullptr, "Failed to create application window.", "Error", MB_ICONERROR | MB_OK);
		return 1;
	}

	// Testing Win32 window attributes
	/*{
		auto attribute = DWMWCP_DONOTROUND;
		DwmSetWindowAttribute(window->get_hwnd(), DWMWA_WINDOW_CORNER_PREFERENCE, &attribute, sizeof(attribute));
	}*/

	dx11 = std::make_unique<renderer::d3d11_renderer>(application.get());

	if (!dx11->init()) {
		MessageBoxA(nullptr, "Failed to initialize D3D11 renderer.", "Error", MB_ICONERROR | MB_OK);
		return 1;
	}

	dx11->set_vsync(false);
	dx11->set_clear_color({88, 88, 88});//({88, 122, 202});

	segoe_font = dx11->register_font("Segoe UI Emoji", 32, FW_THIN, true);

	std::thread draw(draw_thread);

	application->set_visibility(true);

	MSG msg{};
	while (!close_requested && msg.message != WM_QUIT) {
		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_NULL && !IsWindow(application->get_hwnd())) {
			close_requested = true;
			break;
		}

		// TODO: Fix issues with resize
		if (update_size) {
			dx11->resize();
			dx11->reset();

			update_size = false;
		}

		dx11->draw();
		draw_count++;

		updated_draw.notify();
		updated_buf.wait();
	}

	draw.join();

	dx11->release();
	application->destroy();

	return 0;
}