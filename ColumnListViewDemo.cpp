/*
 * ColumnListViewDemo - a small test bench for the "latch" (the expand /
 * collapse disclosure widget) in Haiku's private BColumnListView.
 *
 * The window shows a three-level tree on the left and a set of experiments
 * on the right. Every experiment writes what actually happened into the log,
 * so the log is the answer to "can the latch do X?".
 *
 * Written to stay gcc2-friendly (no C++11 features).
 *
 * Distributed under the terms of the MIT License.
 */


#include <Application.h>
#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <Spinner.h>
#include <String.h>
#include <StringView.h>
#include <TextView.h>
#include <Window.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// private Interface Kit headers (headers/private/interface)
#include <ColumnListView.h>
#include <ColumnTypes.h>


static const char* kAppSignature
	= "application/x-vnd.ScottMc-ColumnListViewDemo";

// messages
static const uint32 kMsgLog				= 'logm';
static const uint32 kMsgStyle			= 'styl';
static const uint32 kMsgLatchWidth		= 'ltwd';
static const uint32 kMsgLazyHasLatch	= 'lzhl';
static const uint32 kMsgExpandAll		= 'exal';
static const uint32 kMsgCollapseAll		= 'coal';
static const uint32 kMsgExpandSel		= 'exsl';
static const uint32 kMsgCollapseSel		= 'cosl';
static const uint32 kMsgMoveNameEnd		= 'mvne';
static const uint32 kMsgMoveNameStart	= 'mvns';
static const uint32 kMsgMoveSizeStart	= 'mvss';
static const uint32 kMsgHideName		= 'hdnm';
static const uint32 kMsgSorting			= 'sort';
static const uint32 kMsgMultiSelect		= 'mult';
static const uint32 kMsgTallRows		= 'tall';
static const uint32 kMsgRebuild			= 'rbld';
static const uint32 kMsgClearLog		= 'clrl';

// logical field indexes (also the order the columns are added in)
enum {
	kFieldName = 0,
	kFieldKind,
	kFieldSize,
	kFieldDepth,
	kFieldModified
};

enum {
	kStyleDefault = 0,
	kStyleBox,
	kStylePerRow
};


// #pragma mark - DemoRow


/*!	A normal row, plus an optional "lazy" flag. Lazy folders have no children
	until they are expanded for the first time. They exist to test whether the
	list consults the virtual BRow::HasLatch() (so that a latch can be shown
	on a row that has no children yet).
*/
class DemoRow : public BRow {
public:
	DemoRow(bool lazy)
		:
		BRow(),
		fLazy(lazy),
		fLoaded(false)
	{
	}

	DemoRow(float height, bool lazy)
		:
		BRow(height),
		fLazy(lazy),
		fLoaded(false)
	{
	}

	virtual bool HasLatch() const
	{
		if (fLazy && sLazyHasLatch)
			return true;
		return BRow::HasLatch();
	}

	bool IsLazy() const			{ return fLazy; }
	bool IsLoaded() const		{ return fLoaded; }
	void SetLoaded()			{ fLoaded = true; }

	static DemoRow* Create(bool lazy)
	{
		if (sRowHeight > 0)
			return new DemoRow(sRowHeight, lazy);
		return new DemoRow(lazy);
	}

	static bool		sLazyHasLatch;
	static float	sRowHeight;		// 0 == BRow's default height

private:
	bool			fLazy;
	bool			fLoaded;
};


bool DemoRow::sLazyHasLatch = true;
float DemoRow::sRowHeight = 0;


static const char*
RowName(BRow* row)
{
	BStringField* field = static_cast<BStringField*>(row->GetField(kFieldName));
	return field != NULL ? field->String() : "?";
}


static const char*
LatchTypeName(LatchType type)
{
	switch (type) {
		case B_NO_LATCH:		return "B_NO_LATCH";
		case B_OPEN_LATCH:		return "B_OPEN_LATCH";
		case B_PRESSED_LATCH:	return "B_PRESSED_LATCH";
		case B_CLOSED_LATCH:	return "B_CLOSED_LATCH";
	}
	return "?";
}


// #pragma mark - DemoListView


class DemoListView : public BColumnListView {
public:
								DemoListView(const char* name);

			void				SetLogTarget(const BMessenger& target)
									{ fLogTarget = target; }
			void				SetLatchStyle(int32 style)
									{ fStyle = style; }
			void				SetProgrammatic(bool programmatic)
									{ fProgrammatic = programmatic; }

			// Log the geometry of the next DrawLatch() call.
			void				ArmProbe()	{ fProbeArmed = true; }

			void				Log(const char* format, ...);

			DemoRow*			AddFolder(const char* name, DemoRow* parent,
									int32 depth, bool lazy = false);
			DemoRow*			AddFile(const char* name, off_t size,
									DemoRow* parent, int32 depth);
			void				BuildSampleTree();

	virtual	void				DrawLatch(BView* view, BRect frame,
									LatchType type, BRow* row);
	virtual	void				ExpandOrCollapse(BRow* row, bool expand);
	virtual	void				SelectionChanged();
	virtual	void				ItemInvoked();

private:
			void				_FillRow(BRow* row, const char* name,
									const char* kind, off_t size, int32 depth);
			void				_LoadLazyChildren(DemoRow* parent);
			void				_DrawBoxLatch(BView* view, BRect frame,
									LatchType type, BRow* row);
			void				_DrawRoundLatch(BView* view, BRect frame,
									LatchType type);

			BMessenger			fLogTarget;
			int32				fStyle;
			bool				fProgrammatic;
			bool				fProbeArmed;
};


DemoListView::DemoListView(const char* name)
	:
	BColumnListView(name, B_WILL_DRAW | B_NAVIGABLE, B_FANCY_BORDER),
	fStyle(kStyleDefault),
	fProgrammatic(false),
	fProbeArmed(false)
{
}


void
DemoListView::Log(const char* format, ...)
{
	char buffer[512];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	BMessage message(kMsgLog);
	message.AddString("text", buffer);
	fLogTarget.SendMessage(&message);
}


void
DemoListView::_FillRow(BRow* row, const char* name, const char* kind,
	off_t size, int32 depth)
{
	time_t now = time(NULL);

	row->SetField(new BStringField(name), kFieldName);
	row->SetField(new BStringField(kind), kFieldKind);
	row->SetField(new BSizeField(size), kFieldSize);
	row->SetField(new BIntegerField(depth), kFieldDepth);
	row->SetField(new BDateField(&now), kFieldModified);
}


DemoRow*
DemoListView::AddFolder(const char* name, DemoRow* parent, int32 depth,
	bool lazy)
{
	DemoRow* row = DemoRow::Create(lazy);
	_FillRow(row, name, lazy ? "Lazy folder" : "Folder", 0, depth);
	AddRow(row, parent);
	return row;
}


DemoRow*
DemoListView::AddFile(const char* name, off_t size, DemoRow* parent,
	int32 depth)
{
	DemoRow* row = DemoRow::Create(false);
	_FillRow(row, name, "File", size, depth);
	AddRow(row, parent);
	return row;
}


void
DemoListView::BuildSampleTree()
{
	// NOTE: Clear() is assumed to delete the rows it holds. If it does not,
	// the old rows leak when the tree is rebuilt - fine for a test program.
	Clear();

	DemoRow* docs = AddFolder("Documents", NULL, 0);
	DemoRow* letters = AddFolder("Letters", docs, 1);
	AddFile("to-mum.txt", 1200, letters, 2);
	AddFile("to-dad.txt", 2400, letters, 2);
	DemoRow* reports = AddFolder("Reports", docs, 1);
	AddFile("2025.pdf", 482000, reports, 2);
	AddFile("2026.pdf", 913000, reports, 2);
	AddFile("readme.txt", 300, docs, 1);

	DemoRow* music = AddFolder("Music", NULL, 0);
	DemoRow* album = AddFolder("Some Album", music, 1);
	DemoRow* disc = AddFolder("Disc 1", album, 2);
	AddFile("01 - Intro.flac", 24100000, disc, 3);
	AddFile("02 - Song.flac", 31800000, disc, 3);
	AddFile("cover.jpg", 220000, album, 2);

	AddFolder("Empty folder (no children, no latch expected)", NULL, 0);
	AddFolder("Lazy folder (children appear on first expand)", NULL, 0, true);

	AddFile("A file with quite a long name to see how the Name column "
		"clips next to the indent and the latch.txt", 42, NULL, 0);
}


void
DemoListView::_LoadLazyChildren(DemoRow* parent)
{
	static const char* kNames[] = { "lazy-1.txt", "lazy-2.txt", "lazy-3.txt" };
	for (int32 i = 0; i < 3; i++)
		AddFile(kNames[i], 100 * (i + 1), parent, 1);
	parent->SetLoaded();
	Log("  (lazy folder loaded 3 children)");
}


void
DemoListView::ExpandOrCollapse(BRow* row, bool expand)
{
	Log("ExpandOrCollapse(\"%s\", %s)  [%s]", RowName(row),
		expand ? "expand" : "collapse",
		fProgrammatic ? "called from a button" : "called by the list itself");

	DemoRow* demoRow = static_cast<DemoRow*>(row);
	if (expand && demoRow->IsLazy() && !demoRow->IsLoaded())
		_LoadLazyChildren(demoRow);

	BColumnListView::ExpandOrCollapse(row, expand);
}


void
DemoListView::SelectionChanged()
{
	BRow* row = CurrentSelection(NULL);
	if (row == NULL)
		Log("SelectionChanged: nothing selected");
	else
		Log("SelectionChanged: \"%s\"  (HasLatch=%s, IsExpanded=%s)",
			RowName(row), row->HasLatch() ? "yes" : "no",
			row->IsExpanded() ? "yes" : "no");
}


void
DemoListView::ItemInvoked()
{
	BRow* row = CurrentSelection(NULL);
	Log("ItemInvoked (double-click / Enter): \"%s\"",
		row != NULL ? RowName(row) : "-");
}


void
DemoListView::DrawLatch(BView* view, BRect frame, LatchType type, BRow* row)
{
	if (fProbeArmed) {
		fProbeArmed = false;
		Log("DrawLatch probe: \"%s\" %s frame=(%.0f, %.0f)-(%.0f, %.0f)  "
			"w=%.0f h=%.0f  LatchWidth()=%.0f",
			row != NULL ? RowName(row) : "-", LatchTypeName(type),
			frame.left, frame.top, frame.right, frame.bottom,
			frame.Width() + 1, frame.Height() + 1, LatchWidth());
	}

	switch (fStyle) {
		case kStyleBox:
			_DrawBoxLatch(view, frame, type, row);
			break;

		case kStylePerRow:
			if (row != NULL && static_cast<DemoRow*>(row)->IsLazy())
				_DrawRoundLatch(view, frame, type);
			else
				BColumnListView::DrawLatch(view, frame, type, row);
			break;

		default:
			BColumnListView::DrawLatch(view, frame, type, row);
			break;
	}
}


void
DemoListView::_DrawBoxLatch(BView* view, BRect frame, LatchType type,
	BRow* row)
{
	if (type == B_NO_LATCH)
		return;

	bool showPlus = type == B_CLOSED_LATCH
		|| (type == B_PRESSED_LATCH && row != NULL && !row->IsExpanded());

	float cx = floorf((frame.left + frame.right) / 2);
	float cy = floorf((frame.top + frame.bottom) / 2);
	BRect box(cx - 4, cy - 4, cx + 4, cy + 4);

	view->PushState();
	if (type == B_PRESSED_LATCH) {
		view->SetHighColor(tint_color(ui_color(B_LIST_BACKGROUND_COLOR),
			B_DARKEN_2_TINT));
	} else
		view->SetHighColor(ui_color(B_LIST_BACKGROUND_COLOR));
	view->FillRect(box);

	view->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
	view->StrokeRect(box);
	view->StrokeLine(BPoint(cx - 2, cy), BPoint(cx + 2, cy));
	if (showPlus)
		view->StrokeLine(BPoint(cx, cy - 2), BPoint(cx, cy + 2));
	view->PopState();
}


void
DemoListView::_DrawRoundLatch(BView* view, BRect frame, LatchType type)
{
	if (type == B_NO_LATCH)
		return;

	float cx = floorf((frame.left + frame.right) / 2);
	float cy = floorf((frame.top + frame.bottom) / 2);
	BRect circle(cx - 4, cy - 4, cx + 4, cy + 4);

	view->PushState();
	view->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
	if (type == B_OPEN_LATCH || type == B_PRESSED_LATCH)
		view->FillEllipse(circle);
	else
		view->StrokeEllipse(circle);
	view->PopState();
}


// #pragma mark - DemoWindow


class DemoWindow : public BWindow {
public:
								DemoWindow();

	virtual	void				MessageReceived(BMessage* message);

private:
			BStringView*		_Heading(const char* text);
			BButton*			_Button(const char* label, uint32 what);
			BCheckBox*			_CheckBox(const char* label, uint32 what,
									bool on);
			BMenuItem*			_StyleItem(const char* label, int32 style);

			void				_AppendLog(const char* text);
			void				_ExpandAll(BRow* parent, bool expand);
			void				_MoveColumn(BColumn* column, int32 index,
									const char* description);

			DemoListView*		fList;
			BColumn*			fNameColumn;
			BColumn*			fSizeColumn;
			BSpinner*			fLatchWidth;
			BTextView*			fLog;
};


DemoWindow::DemoWindow()
	:
	BWindow(BRect(80, 80, 1060, 640), "ColumnListView latch demo",
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE)
{
	fList = new DemoListView("demoList");
	fList->SetExplicitMinSize(BSize(380, 260));
	fList->SetLogTarget(BMessenger(this));

	fNameColumn = new BStringColumn("Name", 230, 60, 600, B_TRUNCATE_END);
	fSizeColumn = new BSizeColumn("Size", 80, 40, 200, B_ALIGN_RIGHT);
	fList->AddColumn(fNameColumn, kFieldName);
	fList->AddColumn(new BStringColumn("Kind", 90, 40, 300, B_TRUNCATE_END),
		kFieldKind);
	fList->AddColumn(fSizeColumn, kFieldSize);
	fList->AddColumn(new BIntegerColumn("Depth", 50, 30, 100, B_ALIGN_RIGHT),
		kFieldDepth);
	fList->AddColumn(new BDateColumn("Modified", 130, 60, 300), kFieldModified);
	fList->SetSelectionMode(B_MULTIPLE_SELECTION_LIST);
	fList->SetSortingEnabled(false);
	fList->BuildSampleTree();

	// latch style menu
	BPopUpMenu* styleMenu = new BPopUpMenu("style");
	BMenuItem* defaultItem = _StyleItem("Default triangle", kStyleDefault);
	styleMenu->AddItem(defaultItem);
	styleMenu->AddItem(_StyleItem("Custom: plus/minus box", kStyleBox));
	styleMenu->AddItem(_StyleItem("Custom: circle on lazy rows only",
		kStylePerRow));
	defaultItem->SetMarked(true);
	BMenuField* styleField = new BMenuField("style", "Drawing:", styleMenu);

	fLatchWidth = new BSpinner("latchWidth", "Latch width:",
		new BMessage(kMsgLatchWidth));
	fLatchWidth->SetRange(0, 60);
	fLatchWidth->SetValue(15);

	fLog = new BTextView("log");
	fLog->MakeEditable(false);
	fLog->SetWordWrap(true);
	BScrollView* logScroll = new BScrollView("logScroll", fLog, 0, false, true,
		B_FANCY_BORDER);
	logScroll->SetExplicitMinSize(BSize(300, 180));

	BLayoutBuilder::Group<>(this, B_HORIZONTAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(fList, 3.0f)
		.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING, 2.0f)
			.Add(_Heading("Latch"))
			.Add(styleField)
			.Add(fLatchWidth)
			.Add(_CheckBox("Lazy rows override HasLatch()", kMsgLazyHasLatch,
				true))
			.AddStrut(B_USE_SMALL_SPACING)
			.Add(_Heading("Expand / collapse from buttons"))
			.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
				.Add(_Button("Expand all", kMsgExpandAll))
				.Add(_Button("Collapse all", kMsgCollapseAll))
				.End()
			.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
				.Add(_Button("Expand selected", kMsgExpandSel))
				.Add(_Button("Collapse selected", kMsgCollapseSel))
				.End()
			.AddStrut(B_USE_SMALL_SPACING)
			.Add(_Heading("Columns"))
			.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
				.Add(_Button("Name to end", kMsgMoveNameEnd))
				.Add(_Button("Name to start", kMsgMoveNameStart))
				.End()
			.Add(_Button("Size to start", kMsgMoveSizeStart))
			.Add(_CheckBox("Hide Name column", kMsgHideName, false))
			.AddStrut(B_USE_SMALL_SPACING)
			.Add(_Heading("List"))
			.Add(_CheckBox("Sorting enabled (click a header)", kMsgSorting,
				false))
			.Add(_CheckBox("Multiple selection", kMsgMultiSelect, true))
			.Add(_CheckBox("Tall rows (28px, applies on rebuild)",
				kMsgTallRows, false))
			.Add(_Button("Rebuild tree", kMsgRebuild))
			.AddStrut(B_USE_SMALL_SPACING)
			.Add(_Heading("Log"))
			.Add(logScroll, 2.0f)
			.Add(_Button("Clear log", kMsgClearLog))
			.End()
		.End();

	_AppendLog("Things to try:\n"
		"1. Click latches, press Left/Right on a selected row. Compare the "
		"log with the buttons: does the list call ExpandOrCollapse() itself?\n"
		"2. Change the latch width (0, 40). Does the latch move, the indent "
		"move, or both?\n"
		"3. Move Name to the end, or Size to the start. Which column gets "
		"the latch and the indent?\n"
		"4. Hide the Name column.\n"
		"5. Lazy folder: toggle the HasLatch() override, then expand it by "
		"click and by button.\n"
		"6. Try the custom drawing styles. Can DrawLatch() change the look? "
		"Does the clickable area change with it?\n"
		"7. Turn on sorting and click headers: do children stay under "
		"their parents?\n");
}


BStringView*
DemoWindow::_Heading(const char* text)
{
	BStringView* view = new BStringView(NULL, text);
	BFont font(be_bold_font);
	view->SetFont(&font);
	return view;
}


BButton*
DemoWindow::_Button(const char* label, uint32 what)
{
	return new BButton(label, new BMessage(what));
}


BCheckBox*
DemoWindow::_CheckBox(const char* label, uint32 what, bool on)
{
	BCheckBox* box = new BCheckBox(label, new BMessage(what));
	box->SetValue(on ? B_CONTROL_ON : B_CONTROL_OFF);
	return box;
}


BMenuItem*
DemoWindow::_StyleItem(const char* label, int32 style)
{
	BMessage* message = new BMessage(kMsgStyle);
	message->AddInt32("style", style);
	return new BMenuItem(label, message);
}


void
DemoWindow::_AppendLog(const char* text)
{
	if (fLog->TextLength() > 30000)
		fLog->Delete(0, 10000);

	BString line(text);
	line << "\n";
	fLog->Insert(fLog->TextLength(), line.String(), line.Length());
	fLog->ScrollToOffset(fLog->TextLength());
}


void
DemoWindow::_ExpandAll(BRow* parent, bool expand)
{
	int32 count = fList->CountRows(parent);
	for (int32 i = 0; i < count; i++) {
		BRow* row = fList->RowAt(i, parent);
		if (row == NULL)
			continue;

		// Only rows that claim to have a latch are touched. A lazy folder
		// without the HasLatch() override is therefore skipped.
		if (expand && row->HasLatch())
			fList->ExpandOrCollapse(row, true);

		_ExpandAll(row, expand);

		if (!expand && row->HasLatch())
			fList->ExpandOrCollapse(row, false);
	}
}


void
DemoWindow::_MoveColumn(BColumn* column, int32 index, const char* description)
{
	fList->MoveColumn(column, index);
	fList->ArmProbe();
	fList->Refresh();
	_AppendLog(description);
}


void
DemoWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgLog:
		{
			const char* text;
			if (message->FindString("text", &text) == B_OK)
				_AppendLog(text);
			break;
		}

		case kMsgClearLog:
			fLog->SetText("");
			break;

		case kMsgStyle:
		{
			int32 style;
			if (message->FindInt32("style", &style) != B_OK)
				break;
			fList->SetLatchStyle(style);
			fList->ArmProbe();
			fList->Refresh();
			_AppendLog("Latch drawing style changed.");
			break;
		}

		case kMsgLatchWidth:
		{
			float width = (float)fLatchWidth->Value();
			fList->SetLatchWidth(width);
			fList->ArmProbe();
			fList->Refresh();
			_AppendLog(BString().SetToFormat("SetLatchWidth(%.0f)", width)
				.String());
			break;
		}

		case kMsgLazyHasLatch:
		{
			DemoRow::sLazyHasLatch = !DemoRow::sLazyHasLatch;
			fList->Refresh();
			_AppendLog(DemoRow::sLazyHasLatch
				? "Lazy rows now return true from HasLatch()."
				: "Lazy rows now fall back to BRow::HasLatch() "
					"(true only once they have children).");
			break;
		}

		case kMsgExpandAll:
		case kMsgCollapseAll:
			fList->SetProgrammatic(true);
			_ExpandAll(NULL, message->what == kMsgExpandAll);
			fList->SetProgrammatic(false);
			break;

		case kMsgExpandSel:
		case kMsgCollapseSel:
		{
			bool expand = message->what == kMsgExpandSel;
			fList->SetProgrammatic(true);
			for (BRow* row = fList->CurrentSelection(NULL); row != NULL;
					row = fList->CurrentSelection(row)) {
				fList->ExpandOrCollapse(row, expand);
			}
			fList->SetProgrammatic(false);
			break;
		}

		case kMsgMoveNameEnd:
			_MoveColumn(fNameColumn, fList->CountColumns() - 1,
				"MoveColumn(Name, last). Where did the latch go?");
			break;

		case kMsgMoveNameStart:
			_MoveColumn(fNameColumn, 0, "MoveColumn(Name, 0).");
			break;

		case kMsgMoveSizeStart:
			_MoveColumn(fSizeColumn, 0,
				"MoveColumn(Size, 0). Where did the latch go?");
			break;

		case kMsgHideName:
		{
			bool hide = !fNameColumn->IsVisible();
			fList->SetColumnVisible(fNameColumn, !hide);
			fList->ArmProbe();
			fList->Refresh();
			_AppendLog(hide ? "Name column hidden. Where is the latch now?"
				: "Name column shown again.");
			break;
		}

		case kMsgSorting:
		{
			bool enable = !fList->SortingEnabled();
			fList->SetSortingEnabled(enable);
			_AppendLog(enable ? "Sorting enabled." : "Sorting disabled.");
			break;
		}

		case kMsgMultiSelect:
		{
			bool multi = fList->SelectionMode() == B_SINGLE_SELECTION_LIST;
			fList->SetSelectionMode(multi ? B_MULTIPLE_SELECTION_LIST
				: B_SINGLE_SELECTION_LIST);
			_AppendLog(multi ? "Multiple selection." : "Single selection.");
			break;
		}

		case kMsgTallRows:
			DemoRow::sRowHeight = DemoRow::sRowHeight > 0 ? 0 : 28;
			_AppendLog(DemoRow::sRowHeight > 0
				? "Row height 28 for new rows - press Rebuild tree."
				: "Default row height for new rows - press Rebuild tree.");
			break;

		case kMsgRebuild:
			fList->BuildSampleTree();
			fList->ArmProbe();
			_AppendLog("Tree rebuilt.");
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


// #pragma mark - DemoApp


class DemoApp : public BApplication {
public:
								DemoApp()
									:
									BApplication(kAppSignature)
								{
								}

	virtual	void				ReadyToRun()
								{
									DemoWindow* window = new DemoWindow();
									window->Show();
								}
};


int
main()
{
	DemoApp app;
	app.Run();
	return 0;
}
