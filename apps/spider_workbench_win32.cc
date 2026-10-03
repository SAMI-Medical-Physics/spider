// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

// FLTK-based graphical interface for Spider on MS-Windows.

#include <direct.h>  // _getcwd
#include <windows.h> // AssignProcessToJobObject, CloseHandle,
                     // CP_UTF8, CreateJobObjectA, CreateProcessA,
                     // CREATE_SUSPENDED, DWORD, errno_t, FALSE,
                     // GetExitCodeProcess, GetLastError,
                     // GetModuleFileNameA, GetSystemDirectoryA,
                     // HANDLE, PROCESS_INFORMATION, ResumeThread,
                     // SetConsoleOutputCP, STARTUPINFOA,
                     // TerminateJobObject, TerminateProcess, UINT,
                     // WAIT_ABANDONED, WAIT_FAILED,
                     // WaitForSingleObject, WAIT_OBJECT_0,
                     // WAIT_TIMEOUT

#include <assert.h>
#include <stddef.h> // NULL, size_t
#include <stdio.h>  // fprintf, snprintf, stderr
#include <stdlib.h> // exit, EXIT_FAILURE
#include <string.h> // memcpy
#include <time.h>   // difftime, localtime_s, strftime, struct tm, time, time_t

#include <FL/Enumerations.H> // FL_ALIGN_INSIDE, FL_ALIGN_LEFT,
                             // FL_API_VERSION,
#include <FL/Fl.H>           // Fl::background, Fl::option,
                             // Fl::OPTION_SIMPLE_ZOOM_SHORTCUT,
                             // Fl::run, Fl::scheme
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include <FL/filename.H> // fl_filename_isdir
#include <FL/fl_ask.H>   // fl_alert, fl_choice

#include "append_escape_arg.cc" // AppendEscapeArg

// XXX: When building this executable, the process code page must be
// set to UTF-8 so that the -A Win32 APIs operate in UTF-8; see
// <https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page>.
// You may ask: why not support Unicode using UTF-16 encoding and the
// -W Win32 APIs instead?  FLTK uses UTF-8 encoding, so by using UTF-8
// ourselves we avoid calling MultiByteToWideChar for each Fl_Input
// value.

// The maximum length in characters of the command line in the Win32
// function CreateProcessA, including the terminating null character.
constexpr int kCommandLineMax = 32767;

// The time in seconds between checks for child process termination.
constexpr double kChildProcessPollInterval = 0.1;

struct PowershellPathname
{
  // Full pathname of Windows Powershell 5.1 powershell.exe.
  char buffer[256];
};

struct SpiderPathname
{
  // Full pathname of spider.ps1 in the same directory as this
  // executable.
  char buffer[kCommandLineMax];
};

struct CommandLine
{
  // Command line to run spider.ps1 using the Win32 function
  // CreateProcessA.
  char buffer[kCommandLineMax];
};

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
  Fl_Box* status;

  // spider process info.
  HANDLE child_job_handle;     // handle to the spider job
  HANDLE child_process_handle; // handle to the spider process
  time_t time_start;
  // This is preferable to using a special exit code in
  // TerminateProcess.
  bool stopped_by_user;
};

// The last significant event w.r.t. running spider.
enum class RunStatus
{
  kReady,
  kRunning,
  kFinished,
  kFailed,
  kStopped,
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
      box.label("Running... (see console)");
      break;
    case RunStatus::kFinished:
      box.label("Finished");
      break;
    case RunStatus::kFailed:
      box.label("Failed");
      break;
    case RunStatus::kStopped:
      box.label("Stopped");
      break;
    }
}

void
FatalError()
{
  fl_alert("An unexpected error occurred.  Spider Workbench will exit.");
  exit(EXIT_FAILURE);
}

void
FillPowershellPathname(PowershellPathname& powershell_pathname)
{
  // Windows PowerShell 5.1 is located at: '<system
  // directory>\WindowsPowerShell\v1.0\powershell.exe'.  The system
  // directory is e.g. 'C:\windows\system32' or 'C:\'.  UINT is
  // unsigned int.
  UINT ret = GetSystemDirectoryA(powershell_pathname.buffer,
                                 sizeof(powershell_pathname.buffer));
  if (!ret)
    {
      fprintf(stderr, "GetSystemDirectoryA failed (system error code %lu)\n",
              GetLastError());
      FatalError();
    }
  if (ret > sizeof(powershell_pathname.buffer))
    {
      fprintf(stderr, "GetSystemDirectoryA: insufficient buffer\n");
      FatalError();
    }

  // If the system directory is the root directory (e.g. "C:\\"),
  // there will be 2 consecutive backslashes and that's OK.
  const char rest_of_path[] = "\\WindowsPowerShell\\v1.0\\powershell.exe";
  // ret does not include the terminating null character.
  if (sizeof(powershell_pathname.buffer) - ret < sizeof(rest_of_path))
    {
      fprintf(stderr, "Insufficient buffer (powershell.exe pathname)\n");
      FatalError();
    }
  memcpy(powershell_pathname.buffer + ret, rest_of_path, sizeof(rest_of_path));
}

void
FillSpiderPathname(SpiderPathname& spider_pathname)
{
  // DWORD is unsigned long.
  DWORD ans = GetModuleFileNameA(NULL, spider_pathname.buffer,
                                 sizeof(spider_pathname.buffer));
  if (!ans)
    {
      fprintf(stderr, "GetModuleFileNameA failed (system error code %lu)\n",
              GetLastError());
      FatalError();
    }
  if (ans == sizeof(spider_pathname.buffer))
    {
      fprintf(stderr, "GetModuleFileNameA: insufficient buffer\n");
      FatalError();
    }

  // ans does not include the terminating null character.
  char* p = spider_pathname.buffer + ans; // on terminating '\0'
  while (p != spider_pathname.buffer)
    {
      --p;
      --ans;
      if (*p == '\\')
        {
          break;
        }
    }
  assert(p == spider_pathname.buffer + ans);

  const char rest_of_path[] = "\\spider.ps1";
  if (sizeof(spider_pathname.buffer) - ans < sizeof(rest_of_path))
    {
      fprintf(stderr, "Insufficient buffer (spider.ps1 pathname)\n");
      FatalError();
    }
  memcpy(p, rest_of_path, sizeof(rest_of_path));
}

// Set the command line (command_line.buffer) used to run spider.ps1.
void
SetCommandLine(const Data& data, const PowershellPathname& powershell_pathname,
               CommandLine& command_line)
{
  std::string cmd_line;
  // The null terminator is not an element of std::basic_string.
  cmd_line.reserve(sizeof(command_line.buffer) - 1);

  // CreateProcessA will be called such that it executes
  // powershell_pathname.buffer and not the first white
  // space-delimited token of the command line.
  AppendEscapeArg(powershell_pathname.buffer, cmd_line);
  cmd_line += " -File ";

  // Use a static SpiderPathname because 1/ it is too large for
  // automatic storage duration, and 2/ it can be set once (including
  // calling GetModuleFileNameA once) close to where it is used,
  // rather than in a broader scope like in main.
  static SpiderPathname spider_pathname;
  static bool spider_pathname_filled;
  if (!spider_pathname_filled)
    {
      FillSpiderPathname(spider_pathname);
      spider_pathname_filled = true;
    }

  AppendEscapeArg(spider_pathname.buffer, cmd_line);

  if (data.force->value())
    cmd_line += " -f";
  if (data.verbose->value())
    cmd_line += " -spider_verbose";
  if (data.output_root->size() != 0)
    {
      cmd_line += " -o ";
      // Use data.output_root->value() up to the first null character.
      AppendEscapeArg(data.output_root->value(), cmd_line);
    }
  if (data.spect_dir1->size() != 0)
    {
      cmd_line += ' ';
      AppendEscapeArg(data.spect_dir1->value(), cmd_line);
    }
  if (data.spect_dir2->size() != 0)
    {
      cmd_line += ' ';
      AppendEscapeArg(data.spect_dir2->value(), cmd_line);
    }
  if (data.spect_dir3->size() != 0)
    {
      cmd_line += ' ';
      AppendEscapeArg(data.spect_dir3->value(), cmd_line);
    }
  if (data.spect_dir4->size() != 0)
    {
      cmd_line += ' ';
      AppendEscapeArg(data.spect_dir4->value(), cmd_line);
    }

  // size() of std::basic_string does not include the null terminator.
  if (cmd_line.size() + 1 > sizeof(command_line.buffer))
    {
      fprintf(stderr, "Command line: insufficient buffer\n");
      FatalError();
    }
  if (cmd_line.size() + 1 > kCommandLineMax)
    {
      fprintf(stderr, "Command line is too long for CreateProcessA\n");
      FatalError();
    }
  memcpy(command_line.buffer, cmd_line.c_str(), cmd_line.size() + 1);
}

// Timer callback that checks for termination of the spider process.
void
CheckChildProcessCallback(void* p)
{
  Data* data = (Data*)(p);
  assert(data->child_job_handle != HANDLE{});
  assert(data->child_process_handle != HANDLE{});
  // Doesn't block.  DWORD is unsigned long.
  DWORD event = WaitForSingleObject(data->child_process_handle, 0);
  switch (event)
    {
    case WAIT_OBJECT_0:
      { // a block is used for duration_s initialisation
        DWORD exit_code;
        GetExitCodeProcess(data->child_process_handle, &exit_code);
        // The child process duration is overestimated by up to
        // kChildProcessPollInterval.  Second resolution is
        // sufficient.
        double duration_s = difftime(time(NULL), data->time_start);
        // We give the spider process an exit code of 1 when it is
        // stopped by the user.
        if (exit_code == 0)
          {
            SetStatusLabel(RunStatus::kFinished, *data->status);
            fprintf(stderr, "\nspider finished, duration %.0f s\n\n",
                    duration_s);
          }
        else if (data->stopped_by_user)
          {
            SetStatusLabel(RunStatus::kStopped, *data->status);
            data->stopped_by_user = false;
            fprintf(stderr, "\nspider stopped by user, duration %.0f s\n\n",
                    duration_s);
          }
        else
          {
            SetStatusLabel(RunStatus::kFailed, *data->status);
            fprintf(stderr,
                    "\nspider failed (exit code %lu), duration %.0f s\n\n",
                    exit_code, duration_s);
          }
        CloseHandle(data->child_process_handle);
        CloseHandle(data->child_job_handle);
        data->child_job_handle = {};
        data->child_process_handle = {};
        data->stop_button->deactivate();
        data->run_button->activate();
        return;
      }
    case WAIT_TIMEOUT:
      Fl::repeat_timeout(kChildProcessPollInterval, CheckChildProcessCallback,
                         data);
      return;
    case WAIT_FAILED:
      fprintf(stderr, "WaitForSingleObject failed (error code %lu)\n",
              GetLastError());
      break;
    case WAIT_ABANDONED:
      // Not possible.
      fprintf(stderr, "WaitForSingleObject returned WAIT_ABANDONED\n");
      break;
    default:
      // Not possible.
      fprintf(stderr, "WaitForSingleObject returned %lu\n", event);
    }
  FatalError();
}

// Create a process that executes 'path\to\powershell.exe' with
// command line 'path\to\powershell.exe -File path\to\spider.ps1
// <args>'.  This is the callback for the 'Run' button.  P is from a
// Data pointer.  Creates a job object and assigns the process to it,
// sets Data's child_job_handle and child_process_handle, and adds
// timers to check for process termination.
void
RunCallback(Fl_Widget*, void* p)
{
  Data* data = (Data*)(p);

  assert(data->child_job_handle == HANDLE{});
  assert(data->child_process_handle == HANDLE{});

  // Use a static PowershellPathname because it can be set once
  // (including calling GetSystemDirectoryA once) close to where it is
  // used, rather than in a broader scope like in main.
  static PowershellPathname powershell_pathname;
  static bool powershell_pathname_filled;
  if (!powershell_pathname_filled)
    {
      FillPowershellPathname(powershell_pathname);
      powershell_pathname_filled = true;
    }

  // Use a static CommandLine because it is too large for automatic
  // storage duration.
  static CommandLine command_line;
  SetCommandLine(*data, powershell_pathname, command_line);

  time_t now = time(NULL);

  // Use a job object to terminate processes spawned by the spider
  // process as well as the spider process.
  data->child_job_handle = CreateJobObjectA(NULL, NULL);
  if (data->child_job_handle == NULL)
    {
      fprintf(stderr, "CreateJobObjectA failed (system error code %lu)\n",
              GetLastError());
      FatalError();
    }

  STARTUPINFOA si = { 0 };
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = { 0 };
  if (!CreateProcessA(powershell_pathname.buffer, command_line.buffer,
                      NULL,  // process handle not inheritable
                      NULL,  // thread handle not inheritable
                      FALSE, // set handle inheritance to FALSE

                      // This creation flag avoids the child process P
                      // spawning a process before P is added to the
                      // job.
                      CREATE_SUSPENDED,
                      NULL, // use parent's environment block
                      NULL, // use parent's starting directory
                      &si, &pi))
    {
      fprintf(stderr, "CreateProcessA failed (system error code %lu)\n",
              GetLastError());
      FatalError();
    }

  if (!AssignProcessToJobObject(data->child_job_handle, pi.hProcess))
    {
      fprintf(stderr,
              "AssignProcessToJobObject failed (system error code %lu)\n",
              GetLastError());
      if (!TerminateProcess(pi.hProcess, 1))
        {
          fprintf(stderr, "TerminateProcess failed (error code %lu)\n",
                  GetLastError());
        }
      FatalError();
    }

  if (ResumeThread(pi.hThread) == (DWORD)-1)
    {
      fprintf(stderr, "ResumeThread failed (system error code %lu)\n",
              GetLastError());
      if (!TerminateJobObject(data->child_job_handle, 1))
        {
          fprintf(stderr, "TerminateJobObject failed (error code %lu)\n",
                  GetLastError());
        }
      FatalError();
    }

  CloseHandle(pi.hThread); // no longer needed

  SetStatusLabel(RunStatus::kRunning, *data->status);
  data->child_process_handle = pi.hProcess;
  data->run_button->deactivate();
  data->stop_button->activate();
  data->time_start = now;

  // Start message.
  char timestamp[256];
  struct tm now_result;
  // XXX: This is Microsoft CRT's localtime_s, not C11's.  errno_t is
  // int.
  errno_t err = localtime_s(&now_result, &now);
  if (err)
    {
      fprintf(stderr, "localtime_s failed (error code %d)\n", err);
    }
  else
    {
      strftime(timestamp, sizeof(timestamp), "%a %b %e %T", &now_result);
      fprintf(stderr, "spider started at %s\n\n", timestamp);
    }

  fprintf(stderr, "%s\n", command_line.buffer);

  // Add timer(s) to check for child process termination.
  Fl::add_timeout(kChildProcessPollInterval, CheckChildProcessCallback, data);
}

void
StopCallback(Fl_Widget*, void* p)
{
  // The child process may have terminated and the timer
  // (CheckChildProcessCallback) hasn't ran since it terminated.
  Data* data = (Data*)(p);
  assert(data->child_job_handle != HANDLE{});
  assert(data->child_process_handle != HANDLE{});
  // Doesn't block.  DWORD is unsigned long.
  DWORD event = WaitForSingleObject(data->child_process_handle, 0);
  switch (event)
    {
    case WAIT_TIMEOUT:
      { // a block is used for exit_code initialisation

        // The exit code to be used by the spider child process and
        // its child processes etc. when stopped by the user.  UINT is
        // unsigned int.
        UINT exit_code = 1;
        if (!TerminateJobObject(data->child_job_handle, exit_code))
          {
            fprintf(stderr, "TerminateJobObject failed (error code %lu)\n",
                    GetLastError());
          }
        else
          {
            data->stopped_by_user = true;
            data->stop_button->deactivate();
          }
        break;
      }
    case WAIT_OBJECT_0:
      data->stop_button->deactivate(); // so it still feels responsive
      break;
    case WAIT_FAILED:
      fprintf(stderr, "WaitForSingleObject failed (error code %lu)\n",
              GetLastError());
      break;
    case WAIT_ABANDONED:
      // Not possible.
      fprintf(stderr, "WaitForSingleObject returned WAIT_ABANDONED\n");
      break;
    default:
      // Not possible.
      fprintf(stderr, "WaitForSingleObject returned %lu\n", event);
    }
}

// Open a dialog for choosing a directory.  If NEW_FOLDER_ICON is
// true, a 'New Folder' icon is shown/enabled.
void
ChooseDirectory(Fl_Input& dir_field, bool new_folder_icon = false)
{
  // FLTK's file chooser is nicer than MS-Windows' native file chooser
  // SHBrowseForFolderW from Fl_Native_File_Chooser.
  Fl_File_Chooser chooser(
      fl_filename_isdir(dir_field.value()) ? dir_field.value() : ".", "*",
      Fl_File_Chooser::SINGLE | Fl_File_Chooser::DIRECTORY,
      "Choose a directory");
  if (new_folder_icon)
    chooser.type(Fl_File_Chooser::SINGLE | Fl_File_Chooser::DIRECTORY
                 | Fl_File_Chooser::CREATE);
  chooser.preview(0); // disable it
  chooser.show();
  while (chooser.shown())
    Fl::wait();
  if (chooser.value() == NULL) // cancelled
    return;
  dir_field.value(chooser.value());
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
// created using the dialog.  This is the callback for
// output_root_browse (in main) and the result populates output_root.
// P is from a Data pointer.
void
BrowseOutputRootCallback(Fl_Widget*, void* p)
{
  Data* data = (Data*)(p);
  ChooseDirectory(*data->output_root, true);
}

void
CloseWindowCallback(Fl_Widget* w, void* p)
{
  Data* data = (Data*)(p);
  if (data->child_job_handle == HANDLE{})
    {
      // Calling 'hide' on the main window causes the program to end.
      w->hide();
      return;
    }

  // Doesn't block.  DWORD is unsigned long.
  DWORD event = WaitForSingleObject(data->child_process_handle, 0);
  switch (event)
    {
    case WAIT_OBJECT_0:
      // The child process has terminated and the timer
      // (CheckChildProcessCallback) hasn't ran since it terminated.
      w->hide();
      break;
    case WAIT_TIMEOUT:
      switch (
          fl_choice("A spider process is running; kill it and exit anyway?",
                    "Yes", "No", 0))
        {
        case 0: // yes

          // XXX: The child process may terminate and the timer
          // (CheckChildProcessCallback) may run before the user makes
          // their choice.
          if (data->child_job_handle != HANDLE{})
            {
              // data->child_job_handle is still a valid handle (the
              // timer hasn't ran and closed it yet).
              if (!TerminateJobObject(data->child_job_handle, 1))
                {
                  fprintf(stderr,
                          "TerminateJobObject failed (error code %lu)\n",
                          GetLastError());
                }
            }
          w->hide();
          break;
        case 1:; // no (default)
        }
      break;
    case WAIT_FAILED:
      fprintf(stderr, "WaitForSingleObject failed (error code %lu)\n",
              GetLastError());
      w->hide();
      break;
    case WAIT_ABANDONED:
      // Not possible.
      fprintf(stderr, "WaitForSingleObject returned WAIT_ABANDONED\n");
      w->hide();
      break;
    default:
      // Not possible.
      fprintf(stderr, "WaitForSingleObject returned %lu\n", event);
      w->hide();
    }
}

int
main(int argc, char* argv[])
{
  // Correctly display any Unicode characters when printing the
  // command line used to run spider.ps1.  (elastix sets this too,
  // which affects the second spider.ps1 run onward, and spider.ps1
  // sets it for the first run.)
  SetConsoleOutputCP(CP_UTF8);

  // Widths and heights in pixel units.
  constexpr int margin = 16; // to window edge
  constexpr int gap = 16;    // for unrelated widgets
  constexpr int gap_small = 8;
  constexpr int label_h = 20;
  constexpr int input_h = 26;
  constexpr int control_h = 30; // taller run and stop buttons
  constexpr int control_w = 200;
  constexpr int window_w = 700;

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
  constexpr int status_y = control_y + control_h + gap;
  constexpr int window_h = status_y + label_h + margin;
  constexpr double control_x = (window_w - control_w) / 2.0;
  // Input text box to check box width ratio of 19:1.
  constexpr double part_w = (window_w - (2 * margin) - gap_small) / 20.0;

  // Main window.
  Fl_Window window(window_w, window_h, "Spider Workbench (MS-Windows)");

  {
    char new_title[64];
    int n = snprintf(new_title, sizeof(new_title),
                     "Spider Workbench (MS-Windows) %s", SPIDER_VERSION);
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
    if (_getcwd(cwd, sizeof(cwd)))
      {
        // FLTK's file chooser dialog returns forward slashes in
        // paths.  Follow this convention.
        for (char* p = cwd; *p; ++p)
          {
            if (*p == '\\')
              {
                *p = '/';
              }
          }
        output_root.value(cwd);
      }
  }
  Fl_Button output_root_browse(margin + (19 * part_w) + gap_small,
                               output_root_input_y, part_w, input_h, "...");

  // Force (-f) and verbose (-spider_verbose) check boxes.
  Fl_Check_Button force(margin, force_y, part_w, label_h,
                        "Ovewrite output files");
  Fl_Check_Button verbose(margin, verbose_y, part_w, label_h, "Verbose");

  // Run and stop buttons.
  Fl_Button run_button(control_x, control_y, (control_w - gap_small) / 2.0,
                       control_h, "Run");
  Fl_Button stop_button(
      control_x + ((control_w - gap_small) / 2.0) + gap_small, control_y,
      (control_w - gap_small) / 2.0, control_h, "Stop");

  // Run status.
  Fl_Box status(margin, status_y, window_w - (2. * margin), label_h);

  window.end();

  Data data{ .spect_dir1 = &spect_dir1,
             .spect_dir2 = &spect_dir2,
             .spect_dir3 = &spect_dir3,
             .spect_dir4 = &spect_dir4,
             .output_root = &output_root,
             .force = &force,
             .verbose = &verbose,
             .run_button = &run_button,
             .stop_button = &stop_button,
             .status = &status,
             .child_job_handle = {},
             .child_process_handle = {},
             .time_start = 0,
             .stopped_by_user = false };

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

  window.resizable(status);
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
