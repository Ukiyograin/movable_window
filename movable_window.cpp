#include <windows.h>
#include <windowsx.h>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>

// 全局变量
HWND g_hWnd = nullptr;
POINT g_windowPos = {0, 0};  // 当前窗口位置
SIZE g_windowSize = {0, 0};  // 窗口大小
bool g_hasMoved = false;     // 标记是否移动过
std::chrono::steady_clock::time_point g_lastMoveTime;  // 最后一次移动的时间
bool g_resetTriggered = false;  // 标记是否触发重置

// 按键状态跟踪
bool g_keyStates[256] = {false};

// 颜色定义
const COLORREF BG_COLOR = RGB(45, 45, 48);      // 深灰色背景
const COLORREF TEXT_COLOR = RGB(241, 241, 241);  // 浅灰色文字
const COLORREF ACCENT_COLOR = RGB(0, 122, 204);  // 蓝色强调色
const COLORREF KEY_COLOR = RGB(86, 156, 214);    // 按键颜色
const COLORREF WARNING_COLOR = RGB(255, 153, 0); // 警告橙色
const COLORREF BORDER_COLOR = RGB(62, 62, 66);   // 边框颜色

// 函数声明
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void MoveWindowBy(int dx, int dy);
void ResetToCenter();
void UpdateLastMoveTime();
bool ShouldResetPosition();
void UpdateWindowMovement();
void CheckWindowBoundary();
void DrawRoundedRect(HDC hdc, int x, int y, int width, int height, int radius);
void DrawKeyButton(HDC hdc, int x, int y, int size, const wchar_t* text, bool pressed);
void DrawModernButton(HDC hdc, int x, int y, int width, int height, const wchar_t* text, bool active);

// 加载字体
HFONT CreateModernFont(const wchar_t* fontName, int size, bool bold = false)
{
    return CreateFontW(
        size, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        fontName
    );
}

// 主函数 
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 注册窗口类
    const wchar_t CLASS_NAME[] = L"MovableWindowClass";
    
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)CreateSolidBrush(BG_COLOR);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    
    RegisterClassW(&wc);
    
    // 获取屏幕尺寸
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 计算初始位置
    int windowWidth = 600;
    int windowHeight = 450;
    g_windowSize.cx = windowWidth;
    g_windowSize.cy = windowHeight;
    g_windowPos.x = (screenWidth - windowWidth) / 2;
    g_windowPos.y = (screenHeight - windowHeight) / 2;
    
    // 创建窗口 
    g_hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"✨ 可移动窗口控制器 ✨",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        g_windowPos.x,
        g_windowPos.y,
        windowWidth,
        windowHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );
    
    if (g_hWnd == nullptr)
    {
        return 0;
    }
    
    // 显示窗口
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    
    // 初始化时间
    g_lastMoveTime = std::chrono::steady_clock::now();
    
    // 设置定时器
    SetTimer(g_hWnd, 1, 50, nullptr);
    SetTimer(g_hWnd, 2, 100, nullptr);
    
    // 消息循环
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}

// 窗口过程函数
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HFONT hTitleFont = nullptr;
    static HFONT hNormalFont = nullptr;
    static HFONT hSmallFont = nullptr;
    
    switch (uMsg)
    {
    case WM_CREATE:
        // 创建字体
        hTitleFont = CreateModernFont(L"Segoe UI", 28, true);
        hNormalFont = CreateModernFont(L"Segoe UI", 16);
        hSmallFont = CreateModernFont(L"Segoe UI", 12);
        break;
        
    case WM_DESTROY:
        KillTimer(hWnd, 1);
        KillTimer(hWnd, 2);
        
        if (hTitleFont) DeleteObject(hTitleFont);
        if (hNormalFont) DeleteObject(hNormalFont);
        if (hSmallFont) DeleteObject(hSmallFont);
        
        PostQuitMessage(0);
        return 0;
        
    case WM_KEYDOWN:
        if (wParam < 256)
        {
            g_keyStates[wParam] = true;
            UpdateLastMoveTime();
            
            // 空格键重置位置
            if (wParam == VK_SPACE)
            {
                ResetToCenter();
                g_resetTriggered = true;
            }
        }
        return 0;
        
    case WM_KEYUP:
        if (wParam < 256)
        {
            g_keyStates[wParam] = false;
        }
        
        // ESC键不退出，只是取消重置标记
        if (wParam == VK_ESCAPE)
        {
            g_resetTriggered = false;
            InvalidateRect(hWnd, nullptr, TRUE);
        }
        return 0;
        
    case WM_TIMER:
        if (wParam == 1)
        {
            // 更新窗口移动
            UpdateWindowMovement();
            
            // 每5秒检查是否需要重置位置（只有移动到边界外才会触发）
            static int timerCount = 0;
            timerCount++;
            if (timerCount >= 100)  // 50ms * 100 = 5秒
            {
                if (ShouldResetPosition() && g_resetTriggered)
                {
                    ResetToCenter();
                    g_resetTriggered = false;
                }
                timerCount = 0;
            }
        }
        else if (wParam == 2)
        {
            // 检查窗口边界
            CheckWindowBoundary();
        }
        return 0;
        
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            // 双缓冲绘图
            HDC hdcMem = CreateCompatibleDC(hdc);
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            HBITMAP hBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
            
            // 绘制渐变背景
            RECT bgRect = clientRect;
            HBRUSH hBgBrush = CreateSolidBrush(BG_COLOR);
            FillRect(hdcMem, &bgRect, hBgBrush);
            
            // 绘制标题栏效果
            RECT titleBarRect = {0, 0, clientRect.right, 60};
            HBRUSH hTitleBarBrush = CreateSolidBrush(RGB(30, 30, 32));
            FillRect(hdcMem, &titleBarRect, hTitleBarBrush);
            
            // 绘制标题
            HFONT hOldFont = (HFONT)SelectObject(hdcMem, hTitleFont);
            SetTextColor(hdcMem, TEXT_COLOR);
            SetBkMode(hdcMem, TRANSPARENT);
            
            // 使用宽字符绘制
            std::wstring titleText = L"✨ 可移动窗口控制器 ✨";
            RECT titleRect = {0, 20, clientRect.right, 80};
            DrawTextW(hdcMem, titleText.c_str(), -1, &titleRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            
            // 绘制控制说明
            SelectObject(hdcMem, hNormalFont);
            
            // 绘制控制区域背景
            RECT controlRect = {50, 100, clientRect.right - 50, 280};
            HBRUSH hControlBrush = CreateSolidBrush(BORDER_COLOR);
            FillRect(hdcMem, &controlRect, hControlBrush);
            DeleteObject(hControlBrush);
            
            // 绘制控制区域边框
            HPEN hBorderPen = CreatePen(PS_SOLID, 2, ACCENT_COLOR);
            HPEN hOldPen = (HPEN)SelectObject(hdcMem, hBorderPen);
            SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
            Rectangle(hdcMem, controlRect.left, controlRect.top, controlRect.right, controlRect.bottom);
            
            // 绘制方向控制
            int centerX = (controlRect.left + controlRect.right) / 2;
            int centerY = (controlRect.top + controlRect.bottom) / 2;
            int keySize = 60;
            
            // 上方向键
            bool upPressed = g_keyStates['W'] || g_keyStates['w'] || g_keyStates[VK_UP];
            std::wstring upText = upPressed ? L"▲" : L"W/↑";
            DrawKeyButton(hdcMem, centerX, centerY - keySize - 20, keySize, upText.c_str(), upPressed);
            
            // 下方向键
            bool downPressed = g_keyStates['S'] || g_keyStates['s'] || g_keyStates[VK_DOWN];
            std::wstring downText = downPressed ? L"▼" : L"S/↓";
            DrawKeyButton(hdcMem, centerX, centerY + keySize + 20, keySize, downText.c_str(), downPressed);
            
            // 左方向键
            bool leftPressed = g_keyStates['A'] || g_keyStates['a'] || g_keyStates[VK_LEFT];
            std::wstring leftText = leftPressed ? L"◀" : L"A/←";
            DrawKeyButton(hdcMem, centerX - keySize - 20, centerY, keySize, leftText.c_str(), leftPressed);
            
            // 右方向键
            bool rightPressed = g_keyStates['D'] || g_keyStates['d'] || g_keyStates[VK_RIGHT];
            std::wstring rightText = rightPressed ? L"▶" : L"D/→";
            DrawKeyButton(hdcMem, centerX + keySize + 20, centerY, keySize, rightText.c_str(), rightPressed);
            
            // 绘制中心区域提示
            SelectObject(hdcMem, hSmallFont);
            RECT centerTextRect = {centerX - 50, centerY - 20, centerX + 50, centerY + 20};
            SetTextColor(hdcMem, RGB(150, 150, 150));
            DrawTextW(hdcMem, L"斜向移动", -1, &centerTextRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            SelectObject(hdcMem, hOldPen);
            DeleteObject(hBorderPen);
            
            // 绘制功能按钮区域
            int buttonWidth = 180;
            int buttonHeight = 40;
            int buttonY = 300;
            
            // 空格键重置按钮
            bool spacePressed = g_keyStates[VK_SPACE];
            std::wstring spaceText = spacePressed ? L"🔄 重置中..." : L"空格键 - 重置位置";
            DrawModernButton(hdcMem, 
                           (clientRect.right - buttonWidth * 2 - 20) / 2, 
                           buttonY, 
                           buttonWidth, 
                           buttonHeight, 
                           spaceText.c_str(), 
                           spacePressed);
            
            // ESC键提示按钮
            bool escPressed = g_keyStates[VK_ESCAPE];
            DrawModernButton(hdcMem, 
                           (clientRect.right - buttonWidth * 2 - 20) / 2 + buttonWidth + 20, 
                           buttonY, 
                           buttonWidth, 
                           buttonHeight, 
                           L"ESC - 取消重置", 
                           escPressed);
            
            // 绘制状态信息
            SelectObject(hdcMem, hNormalFont);
            RECT statusRect = {50, 360, clientRect.right - 50, clientRect.bottom - 20};
            
            // 状态信息背景
            HBRUSH hStatusBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdcMem, &statusRect, hStatusBrush);
            DeleteObject(hStatusBrush);
            
            // 状态信息边框
            HPEN hStatusPen = CreatePen(PS_SOLID, 1, ACCENT_COLOR);
            SelectObject(hdcMem, hStatusPen);
            SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
            Rectangle(hdcMem, statusRect.left, statusRect.top, statusRect.right, statusRect.bottom);
            
            // 状态文本
            SelectObject(hdcMem, hSmallFont);
            
            wchar_t statusText[256];
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_lastMoveTime).count();
            
            if (g_resetTriggered)
            {
                swprintf(statusText, 256, L"⚠️  窗口已移动到边界外！%d秒后自动重置到中心...", 5 - (int)elapsed);
                SetTextColor(hdcMem, WARNING_COLOR);
            }
            else if (elapsed >= 4)
            {
                swprintf(statusText, 256, L"⏰  %d秒未操作，即将自动重置...", 5 - (int)elapsed);
                SetTextColor(hdcMem, WARNING_COLOR);
            }
            else
            {
                swprintf(statusText, 256, L"✅  窗口控制正常 | 位置: (%d, %d)", g_windowPos.x, g_windowPos.y);
            }
            
            RECT textRect = statusRect;
            textRect.left += 10;
            textRect.right -= 10;
            DrawTextW(hdcMem, statusText, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            
            DeleteObject(hStatusPen);
            
            // 绘制底部提示
            SetTextColor(hdcMem, RGB(120, 120, 120));
            RECT hintRect = {0, clientRect.bottom - 25, clientRect.right, clientRect.bottom};
            DrawTextW(hdcMem, L"💡 提示：只有窗口移动到屏幕外才会自动重置，ESC键取消重置", 
                     -1, &hintRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            // 清理画刷
            DeleteObject(hBgBrush);
            DeleteObject(hTitleBarBrush);
            
            // 恢复并清理
            SelectObject(hdcMem, hOldFont);
            SelectObject(hdc, hOldBitmap);
            
            // 将内存DC内容复制到屏幕DC
            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, hdcMem, 0, 0, SRCCOPY);
            
            // 清理资源
            SelectObject(hdcMem, hOldBitmap);
            DeleteObject(hBitmap);
            DeleteDC(hdcMem);
            
            EndPaint(hWnd, &ps);
        }
        return 0;
        
    case WM_SIZE:
        g_windowSize.cx = LOWORD(lParam);
        g_windowSize.cy = HIWORD(lParam);
        InvalidateRect(hWnd, nullptr, TRUE);
        return 0;
        
    case WM_ERASEBKGND:
        return 1;
    }
    
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// 绘制圆角矩形
void DrawRoundedRect(HDC hdc, int x, int y, int width, int height, int radius)
{
    HPEN hPen = (HPEN)GetCurrentObject(hdc, OBJ_PEN);
    HBRUSH hBrush = (HBRUSH)GetCurrentObject(hdc, OBJ_BRUSH);
    
    RoundRect(hdc, x, y, x + width, y + height, radius, radius);
}

// 绘制按键按钮
void DrawKeyButton(HDC hdc, int centerX, int centerY, int size, const wchar_t* text, bool pressed)
{
    int x = centerX - size / 2;
    int y = centerY - size / 2;
    
    // 按键背景
    COLORREF bgColor = pressed ? KEY_COLOR : RGB(60, 60, 60);
    COLORREF textColor = pressed ? RGB(255, 255, 255) : TEXT_COLOR;
    COLORREF borderColor = pressed ? RGB(255, 255, 255) : ACCENT_COLOR;
    
    // 绘制按键阴影（按下状态）
    if (pressed)
    {
        HBRUSH hShadowBrush = CreateSolidBrush(RGB(40, 40, 40));
        HPEN hShadowPen = CreatePen(PS_SOLID, 1, RGB(40, 40, 40));
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hShadowBrush);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hShadowPen);
        DrawRoundedRect(hdc, x + 2, y + 2, size, size, 10);
        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOldPen);
        DeleteObject(hShadowBrush);
        DeleteObject(hShadowPen);
    }
    
    // 绘制按键背景（圆角矩形）
    HBRUSH hBrush = CreateSolidBrush(bgColor);
    HPEN hPen = CreatePen(PS_SOLID, 2, borderColor);
    
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    
    DrawRoundedRect(hdc, x, y, size, size, 10);
    
    // 绘制按键文字
    SetTextColor(hdc, textColor);
    SetBkMode(hdc, TRANSPARENT);
    
    RECT textRect = {x, y, x + size, y + size};
    HFONT hFont = CreateModernFont(L"Segoe UI", size / 2, true);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    DrawTextW(hdc, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    
    // 恢复原来的对象
    SelectObject(hdc, hOldFont);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    
    DeleteObject(hFont);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

// 绘制现代风格按钮
void DrawModernButton(HDC hdc, int x, int y, int width, int height, const wchar_t* text, bool active)
{
    // 按钮背景
    COLORREF bgColor = active ? ACCENT_COLOR : RGB(70, 70, 70);
    COLORREF textColor = active ? RGB(255, 255, 255) : TEXT_COLOR;
    
    // 绘制按钮阴影
    HBRUSH hShadowBrush = CreateSolidBrush(RGB(30, 30, 30));
    HPEN hShadowPen = CreatePen(PS_SOLID, 1, RGB(30, 30, 30));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hShadowBrush);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hShadowPen);
    DrawRoundedRect(hdc, x + 2, y + 2, width, height, 8);
    
    // 绘制按钮
    HBRUSH hBrush = CreateSolidBrush(bgColor);
    HPEN hPen = CreatePen(PS_SOLID, 1, active ? RGB(255, 255, 255) : ACCENT_COLOR);
    
    SelectObject(hdc, hBrush);
    SelectObject(hdc, hPen);
    
    DrawRoundedRect(hdc, x, y, width, height, 8);
    
    // 绘制按钮文字
    SetTextColor(hdc, textColor);
    SetBkMode(hdc, TRANSPARENT);
    
    RECT textRect = {x, y, x + width, y + height};
    HFONT hFont = CreateModernFont(L"Segoe UI", 14, true);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    DrawTextW(hdc, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    
    // 恢复原来的对象
    SelectObject(hdc, hOldFont);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    
    DeleteObject(hFont);
    DeleteObject(hBrush);
    DeleteObject(hPen);
    DeleteObject(hShadowBrush);
    DeleteObject(hShadowPen);
}

// 更新窗口移动
void UpdateWindowMovement()
{
    if (!g_hWnd) return;
    
    int dx = 0;
    int dy = 0;
    
    // 计算移动方向
    if (g_keyStates['W'] || g_keyStates['w'] || g_keyStates[VK_UP])
        dy -= 5;
    if (g_keyStates['S'] || g_keyStates['s'] || g_keyStates[VK_DOWN])
        dy += 5;
    if (g_keyStates['A'] || g_keyStates['a'] || g_keyStates[VK_LEFT])
        dx -= 5;
    if (g_keyStates['D'] || g_keyStates['d'] || g_keyStates[VK_RIGHT])
        dx += 5;
    
    // 如果按下了方向键，移动窗口
    if (dx != 0 || dy != 0)
    {
        MoveWindowBy(dx, dy);
        
        // 更新窗口显示
        InvalidateRect(g_hWnd, nullptr, TRUE);
    }
}

// 移动窗口
void MoveWindowBy(int dx, int dy)
{
    if (!g_hWnd || (dx == 0 && dy == 0)) return;
    
    // 获取屏幕尺寸
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 计算新位置
    g_windowPos.x += dx;
    g_windowPos.y += dy;
    
    // 边界检查，确保窗口不会移出屏幕
    if (g_windowPos.x < -g_windowSize.cx + 20) 
        g_windowPos.x = -g_windowSize.cx + 20;
    if (g_windowPos.y < -g_windowSize.cy + 20)
        g_windowPos.y = -g_windowSize.cy + 20;
    if (g_windowPos.x > screenWidth - 20)
        g_windowPos.x = screenWidth - 20;
    if (g_windowPos.y > screenHeight - 20)
        g_windowPos.y = screenHeight - 20;
    
    // 移动窗口
    SetWindowPos(g_hWnd, nullptr, 
                 g_windowPos.x, g_windowPos.y, 
                 0, 0, 
                 SWP_NOZORDER | SWP_NOSIZE);
    
    g_hasMoved = true;
}

// 检查窗口边界
void CheckWindowBoundary()
{
    if (!g_hWnd) return;
    
    // 获取屏幕尺寸
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 检查是否移动到屏幕外（完全不可见）
    bool outOfBounds = false;
    
    // 如果窗口完全在屏幕外
    if (g_windowPos.x + g_windowSize.cx <= 0 ||  // 完全在左边
        g_windowPos.x >= screenWidth ||          // 完全在右边
        g_windowPos.y + g_windowSize.cy <= 0 ||  // 完全在上边
        g_windowPos.y >= screenHeight)           // 完全在下边
    {
        outOfBounds = true;
    }
    
    // 如果移出屏幕，标记需要重置
    if (outOfBounds && !g_resetTriggered)
    {
        g_resetTriggered = true;
        UpdateLastMoveTime();  // 重置计时器
        InvalidateRect(g_hWnd, nullptr, TRUE);
    }
}

// 重置窗口到屏幕中央
void ResetToCenter()
{
    if (!g_hWnd) return;
    
    // 获取屏幕尺寸
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 计算中心位置
    g_windowPos.x = (screenWidth - g_windowSize.cx) / 2;
    g_windowPos.y = (screenHeight - g_windowSize.cy) / 2;
    
    // 移动窗口
    SetWindowPos(g_hWnd, nullptr, 
                 g_windowPos.x, g_windowPos.y, 
                 0, 0, 
                 SWP_NOZORDER | SWP_NOSIZE);
    
    // 重绘窗口
    InvalidateRect(g_hWnd, nullptr, TRUE);
    
    g_hasMoved = false;
    g_resetTriggered = false;
}

// 更新最后一次移动的时间
void UpdateLastMoveTime()
{
    g_lastMoveTime = std::chrono::steady_clock::now();
}

// 检查是否应该重置位置（超过5秒未操作且已触发重置）
bool ShouldResetPosition()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_lastMoveTime).count();
    return elapsed >= 5;
}