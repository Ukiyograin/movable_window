#include <windows.h>
#include <string>
#include <chrono>
#include <thread>

// 全局变量
HWND g_hWnd = nullptr;
POINT g_windowPos = {0, 0};  // 当前窗口位置
SIZE g_windowSize = {0, 0};  // 窗口大小
bool g_hasMoved = false;     // 标记是否移动过
std::chrono::steady_clock::time_point g_lastMoveTime;  // 最后一次移动的时间

// 函数声明
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void MoveWindowBy(int dx, int dy);
void ResetToCenter();
void UpdateLastMoveTime();
bool ShouldResetPosition();

// 字符串转换辅助函数
std::string WStringToString(const std::wstring& wstr)
{
    if (wstr.empty()) return "";
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), 
                                         nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), 
                       &str[0], size_needed, nullptr, nullptr);
    return str;
}

// 主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 注册窗口类
    const char CLASS_NAME[] = "MovableWindowClass";
    
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    RegisterClassA(&wc);
    
    // 获取屏幕尺寸
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 计算初始位置（屏幕中央）
    int windowWidth = 400;
    int windowHeight = 300;
    g_windowSize.cx = windowWidth;
    g_windowSize.cy = windowHeight;
    g_windowPos.x = (screenWidth - windowWidth) / 2;
    g_windowPos.y = (screenHeight - windowHeight) / 2;
    
    // 创建窗口
    g_hWnd = CreateWindowExA(
        0,
        CLASS_NAME,
        "可移动窗口 - 使用WASD或方向键移动 (5秒不操作自动重置)",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,  // 去掉调整大小和最大化按钮
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
    
    // 初始化最后一次移动时间
    g_lastMoveTime = std::chrono::steady_clock::now();
    
    // 设置一个定时器来检查是否需要重置位置
    SetTimer(g_hWnd, 1, 1000, nullptr);  // 每秒检查一次
    
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
    switch (uMsg)
    {
    case WM_DESTROY:
        KillTimer(hWnd, 1);  // 移除定时器
        PostQuitMessage(0);
        return 0;
        
    case WM_KEYDOWN:
        // 处理键盘输入
        switch (wParam)
        {
        case 'W':
        case 'w':
        case VK_UP:
            MoveWindowBy(0, -20);  // 向上移动
            UpdateLastMoveTime();
            break;
            
        case 'S':
        case 's':
        case VK_DOWN:
            MoveWindowBy(0, 20);   // 向下移动
            UpdateLastMoveTime();
            break;
            
        case 'A':
        case 'a':
        case VK_LEFT:
            MoveWindowBy(-20, 0);  // 向左移动
            UpdateLastMoveTime();
            break;
            
        case 'D':
        case 'd':
        case VK_RIGHT:
            MoveWindowBy(20, 0);   // 向右移动
            UpdateLastMoveTime();
            break;
            
        case VK_SPACE:
            // 空格键手动重置到中心
            ResetToCenter();
            UpdateLastMoveTime();
            break;
            
        case VK_ESCAPE:
            // ESC键退出程序
            DestroyWindow(hWnd);
            break;
        }
        return 0;
        
    case WM_TIMER:
        // 定时器事件，检查是否需要重置位置
        if (wParam == 1 && ShouldResetPosition() && g_hasMoved)
        {
            ResetToCenter();
        }
        return 0;
        
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            // 绘制说明文本
            RECT rect;
            GetClientRect(hWnd, &rect);
            
            // 准备说明文本
            const char* lines[] = {
                "控制说明：",
                "W / ↑ : 向上移动",
                "S / ↓ : 向下移动", 
                "A / ← : 向左移动",
                "D / → : 向右移动",
                "空格键 : 重置到屏幕中心",
                "ESC键 : 退出程序",
                "",
                "5秒不操作会自动重置位置"
            };
            
            // 设置文本颜色和背景模式
            SetTextColor(hdc, RGB(0, 0, 0));  // 黑色文本
            SetBkMode(hdc, TRANSPARENT);      // 透明背景
            
            // 计算每行文本的位置
            int lineHeight = 25;
            int startY = 20;
            
            // 绘制每一行文本
            for (int i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
            {
                RECT lineRect = rect;
                lineRect.top = startY + i * lineHeight;
                lineRect.bottom = lineRect.top + lineHeight;
                
                DrawTextA(hdc, lines[i], -1, &lineRect, 
                         DT_CENTER | DT_NOCLIP);
            }
            
            EndPaint(hWnd, &ps);
        }
        return 0;
        
    case WM_SIZE:
        // 更新窗口大小
        g_windowSize.cx = LOWORD(lParam);
        g_windowSize.cy = HIWORD(lParam);
        return 0;
        
    case WM_ERASEBKGND:
        // 擦除背景
        return 1;
    }
    
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// 移动窗口
void MoveWindowBy(int dx, int dy)
{
    if (!g_hWnd) return;
    
    // 获取屏幕尺寸
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 计算新位置
    g_windowPos.x += dx;
    g_windowPos.y += dy;
    
    // 边界检查，确保窗口不会移出屏幕
    if (g_windowPos.x < 0) g_windowPos.x = 0;
    if (g_windowPos.y < 0) g_windowPos.y = 0;
    if (g_windowPos.x + g_windowSize.cx > screenWidth) 
        g_windowPos.x = screenWidth - g_windowSize.cx;
    if (g_windowPos.y + g_windowSize.cy > screenHeight) 
        g_windowPos.y = screenHeight - g_windowSize.cy;
    
    // 移动窗口
    SetWindowPos(g_hWnd, nullptr, 
                 g_windowPos.x, g_windowPos.y, 
                 0, 0, 
                 SWP_NOZORDER | SWP_NOSIZE);
    
    g_hasMoved = true;
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
    UpdateWindow(g_hWnd);
    
    g_hasMoved = false;
}

// 更新最后一次移动的时间
void UpdateLastMoveTime()
{
    g_lastMoveTime = std::chrono::steady_clock::now();
}

// 检查是否应该重置位置（超过5秒未操作）
bool ShouldResetPosition()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_lastMoveTime).count();
    return elapsed >= 5;
}