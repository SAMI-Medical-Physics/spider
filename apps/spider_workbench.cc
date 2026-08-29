// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

// FLTK-based graphical interface for Spider on POSIX.

#include <assert.h>
#include <signal.h>    // killpg, SIGTERM
#include <spawn.h>     // posix_spawn*, POSIX_SPAWN_SETPGROUP
#include <stddef.h>    // NULL, size_t
#include <stdio.h>     // fprintf, perror, snprintf
#include <stdlib.h>    // exit, EXIT_FAILURE
#include <string.h>    // strerror
#include <sys/types.h> // pid_t, ssize_t
#include <sys/wait.h> // waitpid, WEXITSTATUS, WIFEXITED, WIFSIGNALED, WTERMSIG
#include <time.h>   // difftime, localtime_r, strftime, struct tm, time, time_t
#include <unistd.h> // close, getcwd, pipe, read, STDERR_FILENO

#include <FL/Enumerations.H> // FL_ALIGN_CENTER, FL_ALIGN_INSIDE, FL_ALIGN_LEFT,
                             // FL_API_VERSION, FL_COURIER, FL_READ
#include <FL/Fl.H>           // Fl::add_fd, Fl::background, Fl::option,
                             // Fl::OPTION_SIMPLE_ZOOM_SHORTCUT, Fl::remove_fd,
                             // Fl::run, Fl::scheme
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include <FL/filename.H> // fl_filename_isdir
#include <FL/fl_ask.H>   // fl_alert, fl_choice

extern char** environ; // for posix_spawn

// Width in pixels of line numbers in text display widget.
constexpr int kLineNumberWidth = 40;

// A widget's callback often needs to modify objects other than that
// widget.  Such objects are provided to callbacks using this struct.
struct Data
{
  // spider arguments.
  Fl_Input* spect_dir1;
  Fl_Input* spect_dir2;
  Fl_Input* spect_dir3;
  Fl_Input* spect_dir4;
  Fl_Input* output_root;
  Fl_Check_Button* force;
  Fl_Check_Button* verbose;

  Fl_Button* run_button;
  Fl_Button* stop_button;
  Fl_Text_Display* display;
  Fl_Box* status;

  // spider process info.
  pid_t child_pid; // spider process ID
  time_t time_start;
};

// The last significant event w.r.t. running spider.
enum class RunStatus
{
  kReady,
  kRunning,
  kFinished,
  kFailed,
  kSignal,
  kUnknown
};

// Set the box's label to reflect the run status.
void
SetStatusLabel(RunStatus s, Fl_Box& box)
{
  switch (s)
    {
    case RunStatus::kReady:
      box.label("Ready");
      break;
    case RunStatus::kRunning:
      box.label("Running...");
      break;
    case RunStatus::kFinished:
      box.label("Finished");
      break;
    case RunStatus::kFailed:
      box.label("Failed");
      break;
    case RunStatus::kSignal:
      box.label("Terminated by signal");
      break;
    case RunStatus::kUnknown:
      box.label("Unknown");
    }
}

// Append spider standard error bytes from the pipe to the text
// display buffer.  This is the callable for Fl::add_fd; it is called
// when the parent's read end of the pipe becomes readable.  FD is the
// file descriptor for the parent's read end of the pipe and P is from
// a Data pointer.  When FD receives 0 bytes (EOF), child_pid is set
// to 0, stop_button is deactivated, run_button is activated, and
// status is updated.
void
ReadFdCallback(int fd, void* p)
{
  Data* data = (Data*)(p);
  // XXX: BUF may be appended to a Fl_Text_Buffer, so it must be UTF-8
  // and, for FLTK 1.3, null-terminated.
  char buf[4096];
  ssize_t bytes_read = read(fd, buf, sizeof(buf) - 1);
  if (bytes_read == -1)
    {
      perror("read");
      return;
    }
  if (bytes_read > 0)
    {
      buf[bytes_read] = '\0';
      data->display->buffer()->append(buf);
      data->display->insert_position(data->display->buffer()->length());
      data->display->show_insert_position();

      return;
    }

  // Read 0 bytes: EOF.  Means child's write fd of pipe has closed.
  // Finished with parent's read fd of pipe.
  Fl::remove_fd(fd);
  int status = close(fd);
  if (status == -1)
    {
      perror("close");
    }

  // Append a message of our own to the text display buffer.
  // 'waitpid' also allows the kernel to discard the zombie child
  // process.

  // There is negligible delay between the pipe becoming readable and
  // FLTK calling this function.  Second resolution is sufficient.
  double duration_s = difftime(time(NULL), data->time_start);
  char msg[256];
  int stat_val;
  int n;
  if (waitpid(data->child_pid, &stat_val, 0) == -1)
    {
      perror("waitpid");
      SetStatusLabel(RunStatus::kUnknown, *data->status);
      n = snprintf(msg, sizeof(msg), "waitpid failed, duration %.0f s",
                   duration_s);
    }
  else if (WIFEXITED(stat_val))
    {
      // Child terminated normally.
      int res = WEXITSTATUS(stat_val);
      if (res == 0)
        {
          SetStatusLabel(RunStatus::kFinished, *data->status);
          n = snprintf(msg, sizeof(msg), "spider finished, duration %.0f s",
                       duration_s);
        }
      else
        {
          SetStatusLabel(RunStatus::kFailed, *data->status);
          n = snprintf(msg, sizeof(msg),
                       "spider failed (exit status %d), duration %.0f s", res,
                       duration_s);
        }
    }
  else if (WIFSIGNALED(stat_val))
    {
      // Child terminated due to the receipt of a signal that was not
      // caught.
      SetStatusLabel(RunStatus::kSignal, *data->status);
      n = snprintf(msg, sizeof(msg),
                   "spider was terminated by a signal (signal number %d), "
                   "duration %.0f s",
                   WTERMSIG(stat_val), duration_s);
    }
  else
    {
      fprintf(stderr, "waitpid: unexpected stat_val: %d\n", stat_val);
      assert(false && "Unexpected waitpid stat_val");
      fl_alert("You found a bug:\n  waitpid: unexpected stat_val: %d",
               stat_val);
      SetStatusLabel(RunStatus::kUnknown, *data->status);
      n = snprintf(msg, sizeof(msg),
                   "spider terminated, duration %.0f s.  You found a bug: "
                   "waitpid: unexpected stat_val: %d",
                   duration_s, stat_val);
    }

  data->child_pid = 0;
  data->stop_button->deactivate();
  data->run_button->activate();

  // Check 'snprintf' result before using msg.
  if (n < 0)
    {
      // msg might not be null-terminated.
      fprintf(stderr,
              "snprintf: output error (or truncated if glibc < 2.0.6)\n");
    }
  else if ((size_t)n >= sizeof(msg))
    {
      fprintf(stderr, "snprintf: truncated to '%s'\n", msg);
    }
  if (n < 0 || (size_t)n >= sizeof(msg))
    {
      // Don't use msg.
      data->display->buffer()->append("\nspider sent EOF");
      data->display->insert_position(data->display->buffer()->length());
      data->display->show_insert_position();

      return;
    }

  data->display->buffer()->append("\n");
  data->display->buffer()->append(msg);
  data->display->insert_position(data->display->buffer()->length());
  data->display->show_insert_position();

  return;
}

// argv for spider process.  There are at most 10 arguments including
// the terminating null pointer: {SPIDER_PATHNAME, "-f", "-v", "-o",
// <OUTPUT_ROOT>, <SPECT_DIR1>, <SPECT_DIR2>, <SPECT_DIR3>,
// <SPECT_DIR4>, NULL}.
struct SpiderArgv
{
  char const* argv[10];
};

// Construct argv for spider process using the current widget
// state/values.  The return value's argv[0] can be used as the path
// argument to posix_spawn (i.e. it identifies the new process image
// file to execute).
SpiderArgv
MakeSpiderArgv(const Data& data)
{
  SpiderArgv result = {};
  result.argv[0] = SPIDER_PATHNAME;
  int i = 1;
  if (data.force->value())
    result.argv[i++] = "-f";
  if (data.verbose->value())
    result.argv[i++] = "-v";
  if (data.output_root->size() != 0)
    {
      result.argv[i++] = "-o";
      result.argv[i++] = data.output_root->value();
    }
  if (data.spect_dir1->size() != 0)
    result.argv[i++] = data.spect_dir1->value();
  if (data.spect_dir2->size() != 0)
    result.argv[i++] = data.spect_dir2->value();
  if (data.spect_dir3->size() != 0)
    result.argv[i++] = data.spect_dir3->value();
  if (data.spect_dir4->size() != 0)
    result.argv[i++] = data.spect_dir4->value();
  result.argv[i] = NULL;

  return result;
}

// I expect system call errors to be rare, so simply exit instead of
// cleaning up after them and trying again.
void
FatalError()
{
  fl_alert("An unexpected error occurred.  Spider Workbench will exit.");
  exit(EXIT_FAILURE);
}

// Create a spider process and configure its standard error to be
// written to the text display buffer.  The spider process will be in
// a new process group with a process group ID equal to its process ID
// (child_pid).  This is the callback for run_button.  P is from a
// Data pointer.  When this function is called, there should not be an
// active spider process, so child_pid should be 0.  If this function
// does not call exit, run_button is deactivated, stop_button is
// activated, child_pid and status are updated, and display is reset
// with a start message.  The function also arranges that, when the
// spider process ends, child_pid is set to 0, stop_button is
// deactivated, run_button is activated, and status is updated again.
void
RunCallback(Fl_Widget*, void* p)
{
  Data* data = (Data*)(p);

  assert(data->child_pid == 0);

  // File descriptors for read and write ends of a pipe.
  int fildes[2];
  int status;
  status = pipe(fildes);
  if (status == -1)
    {
      perror("pipe");
      FatalError();
    }
  // From here, cleanup is needed for fildes[0] and fildes[1]: close.

  // Actions to be performed by posix_spawn for the child process.
  posix_spawn_file_actions_t actions;
  status = posix_spawn_file_actions_init(&actions);
  if (status != 0)
    {
      fprintf(stderr, "posix_spawn_file_actions_init: %s\n", strerror(status));
      FatalError();
    }
  // From here, cleanup is needed for actions:
  // posix_spawn_file_actions_destroy.

  // Child will not read from the pipe.
  status = posix_spawn_file_actions_addclose(&actions, fildes[0]);
  if (status != 0)
    {
      fprintf(stderr, "posix_spawn_file_actions_addclose: %s\n",
              strerror(status));
      FatalError();
    }

  // Child's stderr will write to the pipe.
  status
      = posix_spawn_file_actions_adddup2(&actions, fildes[1], STDERR_FILENO);
  if (status != 0)
    {
      fprintf(stderr, "posix_spawn_file_actions_adddup2: %s\n",
              strerror(status));
      FatalError();
    }

  // Close the duplicate fd, unless 'pipe' gave us STDERR_FILENO as
  // fildes[1] to begin with.  (E.g. stdout and stderr may have been
  // closed.  But there may also be an X server connection.)
  if (fildes[1] != STDERR_FILENO)
    {
      status = posix_spawn_file_actions_addclose(&actions, fildes[1]);
      if (status != 0)
        {
          fprintf(stderr, "posix_spawn_file_actions_addclose: %s\n",
                  strerror(status));
          FatalError();
        }
    }

  // spider can start other processes; we will need to stop them too.
  // To this end, spawn the child in a new process group with a
  // process group ID equal to its process ID.  See posix_spawn(3).
  posix_spawnattr_t attr;
  status = posix_spawnattr_init(&attr);
  if (status != 0)
    {
      fprintf(stderr, "posix_spawnattr_init: %s\n", strerror(status));
      FatalError();
    }
  // From here, cleanup is needed for attr: posix_spawnattr_destroy.

  status = posix_spawnattr_setpgroup(&attr, 0);
  if (status != 0)
    {
      fprintf(stderr, "posix_spawnattr_setpgroup: %s\n", strerror(status));
      FatalError();
    }
  status = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
  if (status != 0)
    {
      fprintf(stderr, "posix_spawnattr_setflags: %s\n", strerror(status));
      FatalError();
    }

  SpiderArgv child_argv = MakeSpiderArgv(*data);
  // Make it clear what went wrong when running spider_workbench in
  // the build tree instead of spider_workbench-build.
  fprintf(stderr, "[INFO] Executing '%s'\n", child_argv.argv[0]);
  pid_t child_pid;
  time_t now = time(NULL);
  status = posix_spawn(&child_pid, child_argv.argv[0], &actions, &attr,
                       const_cast<char* const*>(child_argv.argv), environ);
  if (status != 0)
    {
      fprintf(stderr, "posix_spawn: %s\n", strerror(status));
      FatalError();
    }
  // From here, cleanup should stop child process: killpg.

  SetStatusLabel(RunStatus::kRunning, *data->status);
  data->child_pid = child_pid;
  data->run_button->deactivate();
  data->stop_button->activate();
  data->time_start = now;
  data->display->buffer()->text("");
  data->display->linenumber_width(kLineNumberWidth); // turn on line numbers

  // Add start message to display buffer.
  char timestamp[256];
  struct tm now_result;
  strftime(timestamp, sizeof(timestamp), "%a %b %e %T",
           localtime_r(&now, &now_result));
  data->display->buffer()->append("spider started at ");
  data->display->buffer()->append(timestamp);
  data->display->buffer()->append("\n\n");

  // TODO: Show the invoked command in the text display widget
  // instead.
  fprintf(stderr,
          "[INFO] Created a child process by executing '%s' with argv = [",
          child_argv.argv[0]);
  for (char const** p = child_argv.argv; *p; ++p)
    {
      if (p != child_argv.argv)
        fputs(", ", stderr);
      fprintf(stderr, "'%s'", *p);
    }
  fputs("]\n", stderr);

  status = posix_spawn_file_actions_destroy(&actions);
  if (status != 0)
    {
      // Probably leaked memory.
      fprintf(stderr, "posix_spawn_file_actions_destroy: %s\n",
              strerror(status));
    }
  status = posix_spawnattr_destroy(&attr);
  if (status != 0)
    {
      // Probably leaked memory.
      fprintf(stderr, "posix_spawnattr_destroy: %s\n", strerror(status));
    }

  // Parent does not write to the pipe.
  status = close(fildes[1]);
  if (status == -1)
    {
      perror("close");
      // Treat this as fatal because the parent's read end of the pipe
      // may never receive EOF.
      if (killpg(child_pid, SIGTERM) == -1)
        {
          perror("killpg");
        }
      FatalError();
    }
  // Parent's read end of the pipe remains open.

  // Invoke ReadFdCallback when the parent's read end of the pipe
  // becomes readable.
  Fl::add_fd(fildes[0], FL_READ, ReadFdCallback, data);
}

// Stop the active spider process.  If the spider process has any
// child processes, they are stopped too.  This is the callback for
// stop_button.  P is from a Data pointer.  This function should not
// be called when there is not an active spider process.
void
StopCallback(Fl_Widget*, void* p)
{
  Data* data = (Data*)(p);
  if (killpg(data->child_pid, SIGTERM) == -1)
    {
      perror("killpg");
    }
}

// Open a dialog for choosing a directory.  If NEW_FOLDER_ICON is
// true, a 'New Folder' icon is shown, if supported by the system.
void
ChooseDirectory(Fl_Input& dir_field, bool new_folder_icon = false)
{
  Fl_Native_File_Chooser chooser;
  chooser.title("Choose a directory");
  if (new_folder_icon)
    chooser.options(Fl_Native_File_Chooser::NEW_FOLDER);
  chooser.type(Fl_Native_File_Chooser::BROWSE_DIRECTORY);
  if (fl_filename_isdir(dir_field.value()))
    chooser.directory(dir_field.value());
  switch (chooser.show())
    {
    case -1:
      fprintf(stderr,
              "void ChooseDirectory(Fl_Input&, bool): "
              "Fl_Native_File_Chooser::show() failed: %s\n",
              chooser.errmsg());
      break;
    case 1:
      break; // cancelled
    default:
      dir_field.value(chooser.filename());
    }
}

// Open a dialog for choosing the first SPECT directory.  This is the
// callback for spect_dir1_browse (in main) and the result populates
// spect_dir1.  P is from a Data pointer.
void
BrowseSpectDir1Callback(Fl_Widget*, void* p)
{
  Data* data = (Data*)(p);
  ChooseDirectory(*data->spect_dir1);
}

void
BrowseSpectDir2Callback(Fl_Widget*, void* p)
{
  Data* data = (Data*)(p);
  ChooseDirectory(*data->spect_dir2);
}

void
BrowseSpectDir3Callback(Fl_Widget*, void* p)
{
  Data* data = (Data*)(p);
  ChooseDirectory(*data->spect_dir3);
}

void
BrowseSpectDir4Callback(Fl_Widget*, void* p)
{
  Data* data = (Data*)(p);
  ChooseDirectory(*data->spect_dir4);
}

// Open a dialog for choosing the output root.  New directories can be
// created using the dialog, if supported by the system.  This is the
// callback for output_root_browse (in main) and the result populates
// output_root.  P is from a Data pointer.
void
BrowseOutputRootCallback(Fl_Widget*, void* p)
{
  Data* data = (Data*)(p);
  ChooseDirectory(*data->output_root, true);
}

// When quitting Spider Workbench, stop any active spider process and
// its children.  This is the callback for the main window; it runs
// when the user tries to close the main window.  P is from a Data
// pointer.
void
CloseWindowCallback(Fl_Widget* w, void* p)
{
  Data* data = (Data*)(p);
  if (data->child_pid == 0)
    {
      w->hide();
      return;
    }

  switch (fl_choice("A spider process is running; kill it and exit anyway?",
                    "Yes", "No", 0))
    {
    case 0: // yes
      // The child process may finish before the user makes their
      // choice.
      if (data->child_pid != 0)
        {
          if (killpg(data->child_pid, SIGTERM) == -1)
            {
              perror("killpg");
            }
        }
      w->hide();
      break;
    case 1:; // no (default)
    }
}

int
main(int argc, char* argv[])
{
  // Widths and heights in pixel units.
  constexpr int margin = 16; // to window edge
  constexpr int gap = 16;    // for unrelated widgets
  constexpr int gap_small = 8;
  constexpr int label_h = 20;
  constexpr int input_h = 26;
  constexpr int control_h = 30; // taller run and stop buttons
  constexpr int control_w = 200;
  constexpr int display_h = 205;                    // about 11 visual lines
  constexpr int display_w = 836 + kLineNumberWidth; // 90 cols FL_COURIER 15 px

  // Widget coordinates.  X=0, Y=0 is the top left corner of the
  // window.
  constexpr int spect_dirs_label_y = margin;
  constexpr int spect_dirs_input_y = spect_dirs_label_y + label_h + gap_small;
  // 4 input SPECT directories.
  constexpr int output_root_label_y
      = spect_dirs_input_y + (4 * input_h) + (3 * gap_small) + gap;
  constexpr int output_root_input_y
      = output_root_label_y + label_h + gap_small;
  constexpr int force_y = output_root_input_y + input_h + gap;
  constexpr int verbose_y = force_y + label_h + gap_small;
  constexpr int control_y = verbose_y + label_h + gap;
  constexpr int display_y = control_y + control_h + gap;
  constexpr int status_y = display_y + display_h + gap;
  constexpr int window_h = status_y + label_h + margin;
  constexpr int window_w = display_w + (2 * margin);
  constexpr double control_x = (window_w - control_w) / 2.0;
  // Input text box to check box width ratio of 19:1.
  constexpr double part_w = (window_w - (2 * margin) - gap_small) / 20.0;

  // Main window.
  Fl_Window window(window_w, window_h, "Spider Workbench");

  {
    char new_title[32];
    int n = snprintf(new_title, sizeof(new_title), "Spider Workbench %s",
                     SPIDER_VERSION);
    if (n < 0 || (size_t)n >= sizeof(new_title))
      assert(false && "failed to add Spider version to window titlebar");
    else
      window.copy_label(new_title);
  }

  // Input SPECT directories (spider operands).
  Fl_Box spect_dirs_label(margin, spect_dirs_label_y, window_w - (2 * margin),
                          label_h, "SPECT directories (?)");
  spect_dirs_label.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
  spect_dirs_label.tooltip(
      "DICOM SOP Class: Positron Emission Tomography Image Storage");

  Fl_Input spect_dir1(margin, spect_dirs_input_y, 19 * part_w, input_h);
  Fl_Button spect_dir1_browse(margin + (19 * part_w) + gap_small,
                              spect_dirs_input_y, part_w, input_h, "...");

  Fl_Input spect_dir2(margin, spect_dirs_input_y + input_h + gap_small,
                      19 * part_w, input_h);
  Fl_Button spect_dir2_browse(margin + (19 * part_w) + gap_small,
                              spect_dirs_input_y + input_h + gap_small, part_w,
                              input_h, "...");

  Fl_Input spect_dir3(margin, spect_dirs_input_y + 2 * (input_h + gap_small),
                      19 * part_w, input_h);
  Fl_Button spect_dir3_browse(margin + (19 * part_w) + gap_small,
                              spect_dirs_input_y + (2 * (input_h + gap_small)),
                              part_w, input_h, "...");

  Fl_Input spect_dir4(margin, spect_dirs_input_y + (3 * (input_h + gap_small)),
                      19 * part_w, input_h);
  Fl_Button spect_dir4_browse(margin + (19 * part_w) + gap_small,
                              spect_dirs_input_y + (3 * (input_h + gap_small)),
                              part_w, input_h, "...");

  // Output root (-o OUTPUT_ROOT).
  Fl_Box output_root_label(margin, output_root_label_y,
                           window_w - (2 * margin), label_h, "Output root");
  output_root_label.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
  Fl_Input output_root(margin, output_root_input_y, 19 * part_w, input_h);
  {
    char cwd[2048]; // FL_PATH_MAX from <FL/filename.H> is 2048
    if (getcwd(cwd, sizeof(cwd)) == NULL)
      perror("getcwd");
    else
      output_root.value(cwd);
  }
  Fl_Button output_root_browse(margin + (19 * part_w) + gap_small,
                               output_root_input_y, part_w, input_h, "...");

  // Force (-f) and verbose (-v) check boxes.
  Fl_Check_Button force(margin, force_y, part_w, label_h,
                        "Ovewrite output files");
  Fl_Check_Button verbose(margin, verbose_y, part_w, label_h, "Verbose");

  // Run and stop buttons.
  Fl_Button run_button(control_x, control_y, (control_w - gap_small) / 2.0,
                       control_h, "Run");
  Fl_Button stop_button(
      control_x + ((control_w - gap_small) / 2.0) + gap_small, control_y,
      (control_w - gap_small) / 2.0, control_h, "Stop");

  // Text display box and its associated buffer for diagnostic
  // messages from spider.
  // XXX: Create Fl_Text_Buffer before Fl_Text_Display on the stack,
  // since the latter's destructor assumes the former is still alive.
  Fl_Text_Buffer buffer(0, 128 * 1024); // reduce reallocs in verbose mode
  buffer.canUndo(0);
  Fl_Text_Display display(margin, display_y, display_w, display_h);
  display.buffer(buffer);
  display.textfont(FL_COURIER); // monospace font
  display.textsize(15);         // default is 14 px
  display.wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
  // Do not show line numbers (leave the width as 0) until a run
  // starts, otherwise it might give the false impression that the box
  // accepts text input.
  display.linenumber_font(FL_COURIER);
  display.linenumber_align(FL_ALIGN_CENTER);

  // Run status.
  Fl_Box status(margin, status_y, window_w - (2. * margin), label_h);

  window.end();

  // We set child_pid to 0 when there is no running child process.  On
  // Linux and BSDs, 'killpg' with process group 0 sends the signal to
  // the sending process's process group.
  Data data{
    .spect_dir1 = &spect_dir1,
    .spect_dir2 = &spect_dir2,
    .spect_dir3 = &spect_dir3,
    .spect_dir4 = &spect_dir4,
    .output_root = &output_root,
    .force = &force,
    .verbose = &verbose,
    .run_button = &run_button,
    .stop_button = &stop_button,
    .display = &display,
    .status = &status,
    .child_pid = 0,
    .time_start = 0,
  };

  // Callbacks.
  spect_dir1_browse.callback(BrowseSpectDir1Callback, &data);
  spect_dir2_browse.callback(BrowseSpectDir2Callback, &data);
  spect_dir3_browse.callback(BrowseSpectDir3Callback, &data);
  spect_dir4_browse.callback(BrowseSpectDir4Callback, &data);
  output_root_browse.callback(BrowseOutputRootCallback, &data);
  run_button.callback(RunCallback, &data);
  stop_button.callback(StopCallback, &data);
  window.callback(CloseWindowCallback, &data);

  // Other initial widget state.
  stop_button.deactivate();
  SetStatusLabel(RunStatus::kReady, *data.status);

  window.resizable(display);
  Fl_Tooltip::delay(0.5f);

#if FL_API_VERSION >= 10400
  // Zoom in with 'C-='.
  Fl::option(Fl::OPTION_SIMPLE_ZOOM_SHORTCUT, true);
#endif

  Fl::scheme("gtk+");
  // From ef-reverie <https://protesilaos.com/emacs/ef-themes>.
  Fl::background(243, 237, 223);

  // Allow using command line arguments to change the appearance,
  // size, and position of the window.  The options are here:
  // <https://www.fltk.org/doc-1.4/classFl.html#a1576b8c9ca3e900daaa5c36ca0e7ae48>.
  window.show(argc, argv);

  return Fl::run();
}
