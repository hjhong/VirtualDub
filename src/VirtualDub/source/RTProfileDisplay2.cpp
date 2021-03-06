//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "stdafx.h"

#include <windows.h>
#include <vd2/system/thread.h>
#include <vd2/system/profile.h>
#include <vd2/system/time.h>
#include <vector>
#include <list>
#include <algorithm>

#include "gui.h"
#include "oshelper.h"
#include "RTProfileDisplay.h"

extern HINSTANCE g_hInst;

const char g_szRTProfileDisplayControl2Name[]="VDRTProfileDisplay2";

bool RegisterRTProfileDisplayControl2();

/////////////////////////////////////////////////////////////////////////////

struct VDThreadEvent {
	uint64	mTimestamp;
	uint32	mScopeId;
	unsigned	mbOpen : 1;
	unsigned	mAncillaryData : 31;
};

struct VDThreadEventBlock {
	enum { N = 1024 };
	VDThreadEvent mEvents[N];
};

struct VDThreadEventInfo {
	uint32 mEventCount;
	uint32 mEventStartIndex;
	vdfastvector<VDThreadEventBlock *> mEventBlocks;
};

class VDEventProfilerW32 : public IVDEventProfiler {
	VDEventProfilerW32(const VDEventProfilerW32&);
	VDEventProfilerW32& operator=(const VDEventProfilerW32&);
public:
	VDEventProfilerW32();
	~VDEventProfilerW32();

	void Attach();
	void Detach();

	void BeginScope(const char *name, uintptr *cache, uint32 data);
	void BeginDynamicScope(const char *name, uintptr *cache, uint32 data);
	void EndScope();
	void ExitThread();

	void Clear();

	uint32 GetThreadCount() const;
	void UpdateThreadInfo(uint32 threadIndex, VDThreadEventInfo& info) const;

	void UpdateScopes(vdfastvector<const char *>& scopes) const;

protected:
	typedef VDThreadEvent Event;
	typedef VDThreadEventBlock EventBlock;

	struct PerThreadInfo {
		VDThreadId	mThreadId;
		vdfastvector<EventBlock *> mEventBlocks;
		EventBlock *mpCurrentBlock;
		VDAtomicInt mCurrentIndex;
		uint32		mStartIndex;

		PerThreadInfo()
			: mThreadId(VDGetCurrentThreadID())
			, mpCurrentBlock(NULL)
			, mCurrentIndex(EventBlock::N)
			, mStartIndex(0)
		{
		}
	};

	uintptr InitScope(const char *name, uintptr *cache);
	uintptr InitDynamicScope(const char *name, uintptr *cache);

	PerThreadInfo *AllocPerThreadInfo();
	bool AllocBlock(PerThreadInfo *pti);

	uint32	mTlsIndex;
	VDAtomicInt	mEnableCount;

	mutable VDCriticalSection mMutex;
	vdfastvector<const char *> mScopes;
	vdfastvector<char *> mDynScopeStrings;

	typedef vdfastvector<PerThreadInfo *> Threads; 
	Threads mThreads;
};

VDEventProfilerW32::VDEventProfilerW32()
	: mTlsIndex(::TlsAlloc())
	, mEnableCount(0)
{
	mScopes.push_back("");
}

VDEventProfilerW32::~VDEventProfilerW32() {
	while(!mThreads.empty()) {
		PerThreadInfo *pti = mThreads.back();

		while(!pti->mEventBlocks.empty()) {
			delete pti->mEventBlocks.back();
			pti->mEventBlocks.pop_back();
		}

		delete pti;
		mThreads.pop_back();
	}

	while(!mDynScopeStrings.empty()) {
		delete mDynScopeStrings.back();
		mDynScopeStrings.pop_back();
	}

	::TlsFree(mTlsIndex);
}

void VDEventProfilerW32::Attach() {
	++mEnableCount;
}

void VDEventProfilerW32::Detach() {
	--mEnableCount;
}

void VDEventProfilerW32::BeginScope(const char *name, uintptr *cache, uint32 data) {
	if (!mEnableCount)
		return;

	uintptr scopeId = *cache;

	if (!scopeId)
		scopeId = InitScope(name, cache);

	PerThreadInfo *pti = (PerThreadInfo *)::TlsGetValue(mTlsIndex);
	if (!pti)
		pti = AllocPerThreadInfo();

	uint32 idx = pti->mCurrentIndex;

	if (idx >= EventBlock::N) {
		if (!AllocBlock(pti))
			return;

		idx = 0;
	}

	Event& ev = pti->mpCurrentBlock->mEvents[idx++];
	ev.mTimestamp = VDGetPreciseTick();
	ev.mScopeId = scopeId;
	ev.mbOpen = true;
	ev.mAncillaryData = data;

	pti->mCurrentIndex = idx;
}

void VDEventProfilerW32::BeginDynamicScope(const char *name, uintptr *cache, uint32 data) {
	if (!mEnableCount)
		return;

	uintptr scopeId = *cache;

	if (!scopeId)
		scopeId = InitDynamicScope(name, cache);

	PerThreadInfo *pti = (PerThreadInfo *)::TlsGetValue(mTlsIndex);
	if (!pti)
		pti = AllocPerThreadInfo();

	uint32 idx = pti->mCurrentIndex;

	if (idx >= EventBlock::N) {
		if (!AllocBlock(pti))
			return;

		idx = 0;
	}

	Event& ev = pti->mpCurrentBlock->mEvents[idx++];
	ev.mTimestamp = VDGetPreciseTick();
	ev.mScopeId = scopeId;
	ev.mbOpen = true;
	ev.mAncillaryData = data;

	pti->mCurrentIndex = idx;
}

void VDEventProfilerW32::EndScope() {
	PerThreadInfo *pti = (PerThreadInfo *)::TlsGetValue(mTlsIndex);

	if (!pti)
		return;

	if (!mEnableCount)
		return;

	uint32 idx = pti->mCurrentIndex;

	if (idx >= EventBlock::N) {
		if (!AllocBlock(pti))
			return;

		idx = 0;
	}

	Event& ev = pti->mpCurrentBlock->mEvents[idx++];
	ev.mTimestamp = VDGetPreciseTick();
	ev.mScopeId = 0;
	ev.mbOpen = false;

	pti->mCurrentIndex = idx;
}

void VDEventProfilerW32::ExitThread() {
}

void VDEventProfilerW32::Clear() {
	mMutex.Lock();
	for(Threads::iterator it(mThreads.begin()), itEnd(mThreads.end()); it != itEnd; ++it) {
		PerThreadInfo *pti = *it;

		while(!pti->mEventBlocks.empty()) {
			EventBlock *b = pti->mEventBlocks.back();

			if (b && b != pti->mpCurrentBlock)
				delete b;

			pti->mEventBlocks.pop_back();
		}

		pti->mEventBlocks.push_back(pti->mpCurrentBlock);
		pti->mStartIndex = pti->mCurrentIndex;
	}
	mMutex.Unlock();
}

uint32 VDEventProfilerW32::GetThreadCount() const {
	mMutex.Lock();
	uint32 n = mThreads.size();
	mMutex.Unlock();

	return n;
}

void VDEventProfilerW32::UpdateThreadInfo(uint32 threadIndex, VDThreadEventInfo& info) const {
	info.mEventBlocks.clear();
	info.mEventCount = 0;
	info.mEventStartIndex = 0;

	mMutex.Lock();
	uint32 n = mThreads.size();
	if (threadIndex < n) {
		PerThreadInfo& pti = *mThreads[threadIndex];

		size_t n1 = info.mEventBlocks.size();
		size_t n2 = pti.mEventBlocks.size();

		if (n1 < n2)
			info.mEventBlocks.insert(info.mEventBlocks.end(), pti.mEventBlocks.begin() + n1, pti.mEventBlocks.end());

		info.mEventCount = VDThreadEventBlock::N * pti.mEventBlocks.size() - VDThreadEventBlock::N + pti.mCurrentIndex - pti.mStartIndex;
		info.mEventStartIndex = pti.mStartIndex;
	}
	mMutex.Unlock();

}

void VDEventProfilerW32::UpdateScopes(vdfastvector<const char *>& scopes) const {
	mMutex.Lock();
	size_t n1 = scopes.size();
	size_t n2 = mScopes.size();

	if (n1 < n2)
		scopes.insert(scopes.end(), mScopes.begin() + n1, mScopes.end());
	mMutex.Unlock();
}

uintptr VDEventProfilerW32::InitScope(const char *name, uintptr *cache) {
	uintptr h;

	vdsynchronized(mMutex) {
		h = *(volatile uintptr *)cache;
		if (!h) {
			h = mScopes.size();
			mScopes.push_back(name);
			*cache = h;
		}
	}

	return h;
}

uintptr VDEventProfilerW32::InitDynamicScope(const char *name, uintptr *cache) {
	uintptr h;

	vdsynchronized(mMutex) {
		h = *(volatile uintptr *)cache;
		if (!h) {
			h = mScopes.size();

			mDynScopeStrings.push_back();
			char *dynName = _strdup(name);
			mDynScopeStrings.back() = dynName;

			mScopes.push_back(dynName);
			*cache = h;
		}
	}

	return h;
}

VDEventProfilerW32::PerThreadInfo *VDEventProfilerW32::AllocPerThreadInfo() {
	PerThreadInfo *pti = new PerThreadInfo;

	::TlsSetValue(mTlsIndex, pti);

	mMutex.Lock();
	mThreads.push_back(pti);
	mMutex.Unlock();

	return pti;
}

bool VDEventProfilerW32::AllocBlock(PerThreadInfo *pti) {
	mMutex.Lock();
	bool enabled = mEnableCount > 0;
	if (enabled) {
		EventBlock *block = new EventBlock;
		pti->mEventBlocks.push_back(block);
		pti->mpCurrentBlock = block;
		pti->mCurrentIndex = 0;
	} else {
		while(!pti->mEventBlocks.empty()) {
			delete pti->mEventBlocks.back();
			pti->mEventBlocks.pop_back();
		}

		::TlsSetValue(mTlsIndex, NULL);

		Threads::iterator it(std::find(mThreads.begin(), mThreads.end(), pti));

		if (it != mThreads.end()) {
			*it = mThreads.back();
			mThreads.pop_back();
		} else {
			VDASSERT(!"Invalid thread profiling context.");
		}

		delete pti;
	}
	mMutex.Unlock();

	return enabled;
}

/////////////////////////////////////////////////////////////////////////////

void VDInitEventProfiler() {
	if (!g_pVDEventProfiler)
		g_pVDEventProfiler = new VDEventProfilerW32;
}

void VDShutdownEventProfiler() {
	if (g_pVDEventProfiler) {
		delete static_cast<VDEventProfilerW32 *>(g_pVDEventProfiler);
		g_pVDEventProfiler = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////

struct VDProfileTrackedEvent {
	uint64	mStartTime;
	uint64	mEndTime;
	uint32	mScopeId;
	uint32	mChildScopes;
	uint32	mAncillaryData;
};

class VDEventProfileThreadTracker {
public:
	VDEventProfileThreadTracker(uint32 index);

	vdspan<VDProfileTrackedEvent> GetEvents() const;

	void Update();

protected:
	const uint32 mThreadIndex;
	uint32 mLastEventCount;

	VDThreadEventInfo mThreadInfo;

	vdfastvector<VDProfileTrackedEvent> mEvents;
	vdfastvector<size_t> mEventStack;
};

VDEventProfileThreadTracker::VDEventProfileThreadTracker(uint32 index)
	: mThreadIndex(index)
	, mLastEventCount(0)
{
}

vdspan<VDProfileTrackedEvent> VDEventProfileThreadTracker::GetEvents() const {
	return mEvents;
}

void VDEventProfileThreadTracker::Update() {
	VDEventProfilerW32 *p = static_cast<VDEventProfilerW32 *>(g_pVDEventProfiler);
	if (!p)
		return;

	p->UpdateThreadInfo(mThreadIndex, mThreadInfo);

	while(mLastEventCount < mThreadInfo.mEventCount) {
		int idx = mLastEventCount + mThreadInfo.mEventStartIndex;
		const VDThreadEvent& ev = mThreadInfo.mEventBlocks[idx / VDThreadEventBlock::N]->mEvents[idx % VDThreadEventBlock::N];

		if (ev.mbOpen) {
			VDProfileTrackedEvent& tev = mEvents.push_back();
			tev.mStartTime = ev.mTimestamp;
			tev.mEndTime = 0;
			tev.mScopeId = ev.mScopeId;
			tev.mChildScopes = 0xFFFFFFFF;
			tev.mAncillaryData = ev.mAncillaryData;

			mEventStack.push_back(mEvents.size() - 1);
		} else if (!mEventStack.empty()) {
			size_t idx = mEventStack.back();
			mEventStack.pop_back();

			VDProfileTrackedEvent& tev = mEvents[idx];
			tev.mEndTime = ev.mTimestamp;
			tev.mChildScopes = mEvents.size() - (idx + 1);
		}

		++mLastEventCount;
	}
}

/////////////////////////////////////////////////////////////////////////////

class VDRTProfileDisplay2 {
public:
	VDRTProfileDisplay2(HWND hwnd);
	~VDRTProfileDisplay2();

	static VDRTProfileDisplay2 *Create(HWND hwndParent, int x, int y, int cx, int cy, UINT id);

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void OnSize(int w, int h);
	void OnPaint();
	void OnSetFont(HFONT hfont, bool bRedraw);
	bool OnKey(INT wParam);
	void OnTimer();

protected:
	void Clear();
	void DeleteThreadProfiles();
	void UpdateThreadProfiles();
	void UpdateScale();
	void SetDisplayResolution(int res);
	void ScrollByPixels(int dx, int dy);

	typedef VDRTProfiler::Event	Event;
	typedef VDRTProfiler::Channel	Channel;

	const HWND mhwnd;
	HFONT		mhfont;
	VDRTProfiler	*mpProfiler;

	int			mDisplayResolution;
	int			mWidth;
	int			mHeight;

	int			mFontHeight;
	int			mFontAscent;
	int			mFontInternalLeading;

	int			mWheelDeltaAccum;
	int			mDragOffsetX;
	int			mDragOffsetY;

	sint64		mBaseTimeOffset;
	sint64		mBasePixelOffset;
	int			mMajorScaleMinWidth;
	int			mMajorScaleInterval;
	int			mMajorScaleDecimalPlaces;

	VDStringA	mTempStr;

	vdfastvector<VDEventProfileThreadTracker *> mThreadProfiles;
	vdfastvector<const char *> mScopes;

	friend bool VDRegisterRTProfileDisplayControl2();
	static ATOM sWndClass;
};

ATOM VDRTProfileDisplay2::sWndClass;

/////////////////////////////////////////////////////////////////////////////

VDRTProfileDisplay2::VDRTProfileDisplay2(HWND hwnd)
	: mhwnd(hwnd)
	, mhfont(0)
	, mpProfiler(0)
	, mDisplayResolution(0)
	, mWidth(1)
	, mHeight(1)
	, mWheelDeltaAccum(0)
	, mBaseTimeOffset(0)
	, mBasePixelOffset(0)
	, mMajorScaleMinWidth(1)
	, mMajorScaleInterval(1)
{
	SetDisplayResolution(-10);
}

VDRTProfileDisplay2::~VDRTProfileDisplay2() {
	DeleteThreadProfiles();
}

VDRTProfileDisplay2 *VDRTProfileDisplay2::Create(HWND hwndParent, int x, int y, int cx, int cy, UINT id) {
	HWND hwnd = CreateWindow(MAKEINTATOM(sWndClass), "", WS_VISIBLE|WS_CHILD, x, y, cx, cy, hwndParent, (HMENU)id, g_hInst, NULL);

	if (hwnd)
		return (VDRTProfileDisplay2 *)GetWindowLongPtr(hwnd, 0);

	return NULL;
}

bool VDRegisterRTProfileDisplayControl2() {
	WNDCLASS wc;

	wc.style		= 0;
	wc.lpfnWndProc	= VDRTProfileDisplay2::StaticWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(VDRTProfileDisplay2 *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= g_szRTProfileDisplayControl2Name;

	VDRTProfileDisplay2::sWndClass = RegisterClass(&wc);
	return VDRTProfileDisplay2::sWndClass != 0;
}

/////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK VDRTProfileDisplay2::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDRTProfileDisplay2 *pThis = (VDRTProfileDisplay2 *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		if (!(pThis = new_nothrow VDRTProfileDisplay2(hwnd)))
			return FALSE;

		SetWindowLongPtr(hwnd, 0, (LONG_PTR)pThis);
		break;

	case WM_NCDESTROY:
		delete pThis;
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	if (pThis)
		return pThis->WndProc(msg, wParam, lParam);
	else
		return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT VDRTProfileDisplay2::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_CREATE:
			OnSetFont(NULL, FALSE);
			UpdateScale();
			SetTimer(mhwnd, 1, 1000, NULL);
			VDSetDialogDefaultIcons(mhwnd);
			VDInitEventProfiler();
			{
				VDEventProfilerW32 *p = static_cast<VDEventProfilerW32 *>(g_pVDEventProfiler);

				p->Attach();
			}
			return 0;

		case WM_DESTROY:
			{
				VDEventProfilerW32 *p = static_cast<VDEventProfilerW32 *>(g_pVDEventProfiler);

				p->Detach();
			}
			return 0;

		case WM_SIZE:
			OnSize(LOWORD(lParam), HIWORD(lParam));
			return 0;

		case WM_PAINT:
			OnPaint();
			return 0;

		case WM_ERASEBKGND:
			return 0;

		case WM_SETFONT:
			OnSetFont((HFONT)wParam, lParam != 0);
			return 0;

		case WM_GETFONT:
			return (LRESULT)mhfont;

		case WM_KEYDOWN:
			if (OnKey(wParam))
				return 0;
			break;

		case WM_TIMER:
			OnTimer();
			return 0;

		case WM_GETDLGCODE:
			return DLGC_WANTARROWS;

		case WM_MOUSEWHEEL:
			{
				int dz = GET_WHEEL_DELTA_WPARAM(wParam);

				mWheelDeltaAccum += dz;

				int dr = mWheelDeltaAccum / WHEEL_DELTA;

				if (dr) {
					mWheelDeltaAccum %= WHEEL_DELTA;
					SetDisplayResolution(mDisplayResolution + dr);
				}
			}
			break;

		case WM_LBUTTONDOWN:
			if (::GetKeyState(VK_MENU) >= 0) {
				::SetFocus(mhwnd);
				break;
			}
			// fall through
		case WM_MBUTTONDOWN:
			mDragOffsetX = (short)LOWORD(lParam);
			mDragOffsetY = (short)HIWORD(lParam);
			::SetCapture(mhwnd);
			::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_SIZEALL)));
			::SetFocus(mhwnd);
			break;

		case WM_RBUTTONDOWN:
			::SetFocus(mhwnd);
			break;

		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
			if (::GetCapture() == mhwnd)
				::ReleaseCapture();
			break;

		case WM_MOUSEMOVE:
			if (::GetCapture() == mhwnd) {
				int x = (short)LOWORD(lParam);
				int y = (short)HIWORD(lParam);
				int dx = x - mDragOffsetX;
				int dy = y - mDragOffsetY;

				mDragOffsetX = x;
				mDragOffsetY = y;

				ScrollByPixels(-dx, -dy);
			}
			break;

		case WM_SETCURSOR:
			if (::GetCapture() == mhwnd) {
				::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_SIZEALL)));
				return TRUE;
			}
			break;

		case WM_SYSKEYDOWN:
			if (wParam == VK_MENU)
				return 0;
			break;

		case WM_SYSKEYUP:
			if (wParam == VK_MENU)
				return 0;
			break;
	}
	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDRTProfileDisplay2::OnSize(int w, int h) {
	mWidth = w;

	if (mHeight != h) {
		mHeight = h;
		::InvalidateRect(mhwnd, NULL, TRUE);
	}
}

void VDRTProfileDisplay2::OnPaint() {
	PAINTSTRUCT ps;
	if (HDC hdc = BeginPaint(mhwnd, &ps)) {
		HBRUSH hbrBackground = (HBRUSH)GetClassLongPtr(mhwnd, GCLP_HBRBACKGROUND);
		HGDIOBJ hOldFont = 0;
		if (mhfont)
			hOldFont = SelectObject(hdc, mhfont);

		RECT rClient;
		GetClientRect(mhwnd, &rClient);

		FillRect(hdc, &rClient, hbrBackground);

		double pixelsPerMicrosec = pow(2.0, (double)mDisplayResolution);
		double microsecsPerPixel = pow(2.0, -(double)mDisplayResolution);
		double pixelsPerTick = VDGetPreciseSecondsPerTick() * 1000000.0 * pixelsPerMicrosec;

		size_t threadCount = mThreadProfiles.size();
		uint64 baseTime = 0;
		bool baseTimeSet = false;

		for(size_t i=0; i<threadCount; ++i) {
			VDEventProfileThreadTracker *tracker = mThreadProfiles[i];

			if (!tracker)
				continue;

			const vdspan<VDProfileTrackedEvent>& events = tracker->GetEvents();
			
			if (!events.empty()) {
				uint64 firstTime = events.front().mStartTime;

				if (!baseTimeSet || (sint64)(firstTime - baseTime) < 0) {
					baseTimeSet = true;
					baseTime = firstTime;
				}
			}
		}

		sint64 timeStartOffset = VDRoundToInt64((ps.rcPaint.left + mBasePixelOffset) * microsecsPerPixel);
		sint64 majorTick = timeStartOffset;

		--majorTick;
		majorTick -= majorTick % mMajorScaleInterval;

		::SetTextAlign(hdc, TA_LEFT | TA_BOTTOM);
		::SetTextColor(hdc, 0);
		for(;;) {
			int x = VDFloorToInt(majorTick * pixelsPerMicrosec) - (int)mBasePixelOffset;

			if (x >= ps.rcPaint.right)
				break;

			RECT r = ps.rcPaint;
			r.left = x;
			r.right = x+1;
			FillRect(hdc, &r, (HBRUSH)(COLOR_GRAYTEXT + 1));

			majorTick += mMajorScaleInterval;

			char buf[64];
			sprintf(buf, "%.*fs", mMajorScaleDecimalPlaces, (double)majorTick / 1000000.0);

			::TextOut(hdc, x + 2, mHeight - 2, buf, strlen(buf));
		}

		::SetTextAlign(hdc, TA_LEFT | TA_TOP);
		for(size_t i=0; i<threadCount; ++i) {
			VDEventProfileThreadTracker *tracker = mThreadProfiles[i];

			if (!tracker)
				continue;

			const vdspan<VDProfileTrackedEvent>& events = tracker->GetEvents();
			size_t n = events.size();

			int y1 = i * 20;
			int y2 = y1 + 20;

			for(uint32 j=0; j<n; ++j) {
				const VDProfileTrackedEvent& ev = events[j];
				if (ev.mChildScopes == 0xFFFFFFFF)
					continue;

				double xf1 = (double)(sint64)(ev.mStartTime - baseTime) * pixelsPerTick;
				double xf2 = (double)(sint64)(ev.mEndTime - baseTime) * pixelsPerTick;

				sint64 x1 = VDCeilToInt64(xf1 - 0.5) - mBasePixelOffset;
				sint64 x2 = VDCeilToInt64(xf2 - 0.5) - mBasePixelOffset;

				if (x1 >= ps.rcPaint.right)
					break;

				if (x2 <= ps.rcPaint.left)
					continue;

				int ix1 = VDClampToSint32(x1);
				int ix2 = VDClampToSint32(x2);

				if (ix2 > ix1) {
					Rectangle(hdc, ix1, y1, ix2, y2);

					const char *s = mScopes[ev.mScopeId];

					if (ix2 - ix1 > 4 && y2 - y1 > 4) {
						RECT inside;
						inside.left = ix1 + 2;
						inside.right = ix2 - 2;
						inside.top = y1 + 2;
						inside.bottom = y2 - 2;

						if (ev.mAncillaryData) {
							mTempStr.sprintf("%s (%u)", s, ev.mAncillaryData);
							DrawText(hdc, mTempStr.data(), mTempStr.size(), &inside, DT_LEFT | DT_VCENTER | DT_NOPREFIX | DT_SINGLELINE);
						} else {
							DrawText(hdc, s, strlen(s), &inside, DT_LEFT | DT_VCENTER | DT_NOPREFIX | DT_SINGLELINE);
						}
					}
				}
			}
		}

		if (hOldFont)
			SelectObject(hdc, mhfont);

		EndPaint(mhwnd, &ps);
	}
}

void VDRTProfileDisplay2::OnTimer() {
	UpdateThreadProfiles();

	InvalidateRect(mhwnd, NULL, TRUE);
	UpdateWindow(mhwnd);
}

void VDRTProfileDisplay2::OnSetFont(HFONT hfont, bool bRedraw) {
	mhfont = hfont;
	if (bRedraw)
		InvalidateRect(mhwnd, NULL, TRUE);

	// stuff in some reasonable defaults if we fail measuring font metrics
	mFontHeight				= 16;
	mFontAscent				= 12;
	mFontInternalLeading	= 0;

	// measure font metrics
	if (HDC hdc = GetDC(mhwnd)) {
		HGDIOBJ hOldFont = 0;
		if (mhfont)
			hOldFont = SelectObject(hdc, mhfont);

		TEXTMETRIC tm;
		if (GetTextMetrics(hdc, &tm)) {
			mFontHeight				= tm.tmHeight;
			mFontAscent				= tm.tmAscent;
			mFontInternalLeading	= tm.tmInternalLeading;
		}

		SIZE sz;
		if (GetTextExtentPoint32(hdc, "00000000", 8, &sz))
			mMajorScaleMinWidth = sz.cx + 10;

		if (hOldFont)
			SelectObject(hdc, mhfont);
	}
}

bool VDRTProfileDisplay2::OnKey(INT wParam) {
	if (wParam == VK_LEFT) {
		SetDisplayResolution(mDisplayResolution - 1);
	} else if (wParam == VK_RIGHT) {
		SetDisplayResolution(mDisplayResolution + 1);
	} else if (wParam == 'X') {
		if (::GetKeyState(VK_CONTROL) < 0) {
			Clear();
			return true;
		}
	}
	return false;
}

void VDRTProfileDisplay2::Clear() {
	VDEventProfilerW32 *p = static_cast<VDEventProfilerW32 *>(g_pVDEventProfiler);

	if (p)
		p->Clear();

	DeleteThreadProfiles();

	mBaseTimeOffset = 0;
	mBasePixelOffset = 0;
	InvalidateRect(mhwnd, NULL, TRUE);
}

void VDRTProfileDisplay2::DeleteThreadProfiles() {
	while(!mThreadProfiles.empty()) {
		VDEventProfileThreadTracker *p = mThreadProfiles.back();
		mThreadProfiles.pop_back();

		delete p;
	}
}

void VDRTProfileDisplay2::UpdateThreadProfiles() {
	VDEventProfilerW32 *p = static_cast<VDEventProfilerW32 *>(g_pVDEventProfiler);

	if (!p)
		return;

	uint32 n = p->GetThreadCount();

	if (n > mThreadProfiles.size())
		mThreadProfiles.resize(n, NULL);

	for(uint32 i=0; i<n; ++i) {
		VDEventProfileThreadTracker *tracker = mThreadProfiles[i];

		if (!tracker) {
			tracker = new VDEventProfileThreadTracker(i);

			mThreadProfiles[i] = tracker;
		}

		tracker->Update();
	}

	p->UpdateScopes(mScopes);
}

void VDRTProfileDisplay2::UpdateScale() {
	double decimalPlaces = ceil(log10((double)(mMajorScaleMinWidth << -mDisplayResolution)));
	mMajorScaleDecimalPlaces = 6 - (int)decimalPlaces;

	if (mMajorScaleDecimalPlaces < 0)
		mMajorScaleDecimalPlaces = 0;

	if (mMajorScaleDecimalPlaces > 6)
		mMajorScaleDecimalPlaces = 6;

	mMajorScaleInterval = VDRoundToInt(pow(10.0, decimalPlaces));
}

void VDRTProfileDisplay2::SetDisplayResolution(int res) {
	if (res > 0)
		res = 0;

	if (res < -20)
		res = -20;

	if (res == mDisplayResolution)
		return;

	int halfw = 0;

	RECT r;
	if (::GetClientRect(mhwnd, &r))
		halfw = r.right >> 1;

	mBaseTimeOffset += (halfw << -mDisplayResolution) - (halfw << -res);
	if (mBaseTimeOffset < 0)
		mBaseTimeOffset = 0;

	mDisplayResolution = res;
	mBasePixelOffset = VDRoundToInt64((double)mBaseTimeOffset * pow(2.0, (double)mDisplayResolution));

	UpdateScale();

	InvalidateRect(mhwnd, NULL, TRUE);
}

void VDRTProfileDisplay2::ScrollByPixels(int dx, int dy) {
	sint64 newOffset = mBaseTimeOffset + (dx << -mDisplayResolution);
	if (newOffset < 0)
		newOffset = 0;

	if (mBaseTimeOffset != newOffset) {
		mBaseTimeOffset = newOffset;

		sint64 pixOffset = VDRoundToInt64((double)mBaseTimeOffset * pow(2.0, (double)mDisplayResolution));

		if (mBasePixelOffset != pixOffset) {
			sint64 pdx = mBasePixelOffset - pixOffset;
			mBasePixelOffset = pixOffset;

			::ScrollWindow(mhwnd, (int)pdx, 0, NULL, NULL);
		}
	}
}
