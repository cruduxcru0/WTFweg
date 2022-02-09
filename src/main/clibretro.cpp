#include "clibretro.h"
#include <string>
#include <iostream>
#include <filesystem>
#include "io.h"
#include "utils.h"
#define INI_STRNICMP(s1, s2, cnt) (strcmp(s1, s2))
#include "ini.h"
#ifdef _WIN32
#include <windows.h>
#endif
using namespace std;

#ifdef _WIN32
static std::string_view SHLIB_EXTENSION = ".dll";
#else
static std::string_view SHLIB_EXTENSION = ".so";
#endif

CLibretro *CLibretro::get_classinstance(SDL_Window *window)
{
  static thread_local CLibretro *instance = new CLibretro(window);
  return instance;
}

void CLibretro::poll()
{
keyboard_binds = SDL_GetKeyboardState(NULL);
}

int CLibretro::getbind(unsigned port, unsigned device, unsigned index,
                                unsigned id)
{
   if (port != 0)
    return 0;

   std::string str =SDL_JoystickNameForIndex(0);

  

  if (device == RETRO_DEVICE_JOYPAD) {
      int16_t value =  keyboard_binds[core_inputbinds[id].sdl_id];
      value = abs(value);
      return value;
      }
  return 0;
}

struct s9x_keys{
  int retro_id;
  int sdl_id;
};
s9x_keys snes9xbinds[] = {
 RETRO_DEVICE_ID_JOYPAD_B,SDL_SCANCODE_C,
RETRO_DEVICE_ID_JOYPAD_Y,		SDL_SCANCODE_X,
RETRO_DEVICE_ID_JOYPAD_SELECT ,	SDL_SCANCODE_SPACE,
RETRO_DEVICE_ID_JOYPAD_START   ,		SDL_SCANCODE_RETURN,
RETRO_DEVICE_ID_JOYPAD_UP, SDL_SCANCODE_UP,
RETRO_DEVICE_ID_JOYPAD_DOWN,  SDL_SCANCODE_DOWN,
RETRO_DEVICE_ID_JOYPAD_LEFT, SDL_SCANCODE_LEFT,
RETRO_DEVICE_ID_JOYPAD_RIGHT, SDL_SCANCODE_RIGHT,
RETRO_DEVICE_ID_JOYPAD_A,		SDL_SCANCODE_D,
RETRO_DEVICE_ID_JOYPAD_X,		SDL_SCANCODE_S,
RETRO_DEVICE_ID_JOYPAD_L,	 SDL_SCANCODE_A,
RETRO_DEVICE_ID_JOYPAD_R,		SDL_SCANCODE_Z,
RETRO_DEVICE_ID_JOYPAD_L2,  0,
RETRO_DEVICE_ID_JOYPAD_R2,    0,
RETRO_DEVICE_ID_JOYPAD_L3 ,   0,
RETRO_DEVICE_ID_JOYPAD_R3 ,  0,
};



bool CLibretro::init_inputvars(retro_input_descriptor* var)
{
  core_inputbinds.clear();
  core_inputbinds.resize(16);


  int num_joy = SDL_NumJoysticks();
  if(num_joy)
  joystick = SDL_JoystickOpen(0);

 
  
  while (var->description != NULL && var->port == 0) {
          core_inputbinds[var->id].description= var->description;
            core_inputbinds[var->id].joystic_guid = SDL_JoystickGetGUID(joystick);
            core_inputbinds[var->id].joystick_name =SDL_JoystickName(joystick);

   




          if (var->device == RETRO_DEVICE_ANALOG ||(var->device == RETRO_DEVICE_JOYPAD)) {
            if (var->device == RETRO_DEVICE_ANALOG) {
                  //bit tortuous but here we go. Basically we do this to enable each RetroArch joypad axis its own button
                  //this comes in handy when using joypads for digital input movements. Or for other controllers.
                  int retro_id =(var->index == RETRO_DEVICE_INDEX_ANALOG_LEFT)? (var->id == RETRO_DEVICE_ID_ANALOG_X ?
                  joypad_analogx_l: joypad_analogx_r): 
                  (var->id == RETRO_DEVICE_ID_ANALOG_X ? joypad_analogy_l : joypad_analogy_r);
                  core_inputbinds[var->id].isanalog = true;

              }
              else if (var->device == RETRO_DEVICE_JOYPAD){
                  core_inputbinds[var->id].sdl_id =  snes9xbinds[var->id].sdl_id;
                  core_inputbinds[var->id].isanalog = false;
              }
               var++;

     }
}
return true;
}

const char *CLibretro::load_corevars(retro_variable *var)
{
  for (size_t i = 0; i < core_variables.size(); i++)
  {

    if (strcmp(core_variables[i].name.c_str(), var->key) == 0)
      return core_variables[i].var.c_str();
  }
  return NULL;
}

bool CLibretro::init_configvars(retro_variable *var)
{
  size_t num_vars = 0;

  std::vector<loadedcore_configvars> variables;
  variables.clear();
  variables_changed = false;

  for (const struct retro_variable *v = var; v->key; ++v)
    num_vars++;

  for (unsigned i = 0; i < num_vars; ++i)
  {
    loadedcore_configvars vars_struct;
    const struct retro_variable *invar = &var[i];

    vars_struct.name = invar->key;
    vars_struct.config_visible = true;
    std::string str1 = invar->value;
    std::string::size_type pos = str1.find(';');
    vars_struct.description = str1.substr(0, pos);
    // skip ; and space to first variable
    pos = str1.find(' ', pos) + 1;
    // get all variables
    str1 = str1.substr(pos, string::npos);
    vars_struct.usevars = str1;

    std::stringstream test(str1);
    std::string segment;
    std::vector<std::string> seglist;
    while (std::getline(test, segment, '|'))
      vars_struct.config_vals.push_back(segment);

    vars_struct.sel_idx = 0;

    pos = str1.find('|');
    // get first variable as default/current
    if (pos != std::string::npos)
      str1 = str1.substr(0, pos);
    vars_struct.var = str1;
    variables.push_back(vars_struct);
  }

  core_variables = variables;
  return true;
}

CLibretro::CLibretro(SDL_Window *window)
{
  memset((retro_core *)&retro, 0, sizeof(retro_core));
  cores.clear();
  get_cores();
  sdl_window = window;
}

CLibretro::~CLibretro()
{
  core_unload();
}

bool CLibretro::core_load(char *ROM, bool game_specific_settings)
{
  const char *str = cores.at(0).core_path.c_str();
  void *hDLL = openlib((const char *)str);
  if (!hDLL)
  {
    return false;
  }

#define libload(name) getfunc(hDLL, name)
#define load_sym(V, name)                         \
  if (!(*(void **)(&V) = (void *)libload(#name))) \
    return false;
#define load_retro_sym(S) load_sym(retro.S, S)
  retro.handle = hDLL;
  load_retro_sym(retro_init);
  load_retro_sym(retro_deinit);
  load_retro_sym(retro_api_version);
  load_retro_sym(retro_get_system_info);
  load_retro_sym(retro_get_system_av_info);
  load_retro_sym(retro_set_controller_port_device);
  load_retro_sym(retro_reset);
  load_retro_sym(retro_run);
  load_retro_sym(retro_load_game);
  load_retro_sym(retro_unload_game);
  load_retro_sym(retro_serialize);
  load_retro_sym(retro_unserialize);
  load_retro_sym(retro_serialize_size);
  load_retro_sym(retro_get_memory_size);
  load_retro_sym(retro_get_memory_data);
  load_envsymb(retro.handle);
  retro.retro_init();
  retro.initialized = true;

  struct retro_system_info system = {0};
  retro_system_av_info av = {0};
  info = {ROM, 0};
  info.path = ROM;
  info.data = NULL;
  info.size = get_filesize(ROM);
  info.meta = "";

  retro.retro_get_system_info(&system);
   retro.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
  if (!system.need_fullpath)
  {
    FILE *inputfile = fopen(ROM, "rb");
    if (!inputfile)
    {
    fail:
      printf("FAILED TO LOAD ROMz!!!!!!!!!!!!!!!!!!");
      return false;
    }
    info.data = malloc(info.size);
    if (!info.data)
      goto fail;
    fread((void *)info.data, 1, info.size, inputfile);
    fclose(inputfile);
    inputfile = NULL;
  }

  if (!retro.retro_load_game(&info))
  {
    printf("FAILED TO LOAD ROM!!!!!!!!!!!!!!!!!!");
    return false;
  }
  retro.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
  retro.retro_get_system_av_info(&av);
  float refreshr = 60;
  audio_init(refreshr, av.timing.sample_rate, av.timing.fps);
  video_init(&av.geometry, refreshr, sdl_window);
  lr_isrunning = true;
  return true;
}

bool CLibretro::core_isrunning()
{
  return lr_isrunning;
}

void CLibretro::core_run()
{
  if (lr_isrunning)
  {

    retro.retro_run();
  }
}

void CLibretro::core_unload()
{
  if (retro.initialized)
  {
    retro.retro_unload_game();
    retro.retro_deinit();
  }
  if (retro.handle)
  {
    freelib(retro.handle);
    retro.handle = NULL;
    memset((retro_core *)&retro, 0, sizeof(retro_core));
  }
  if (info.data)
    free((void *)info.data);
  audio_destroy();

  cores.clear();
}

void addplugin(const char *path, std::vector<core_info> *cores)
{
  typedef void (*retro_get_system_info)(struct retro_system_info * info);
  retro_get_system_info getinfo;
  struct retro_system_info system = {0};

  void *hDLL = openlib(path);
  if (!hDLL)
  {
    return;
  }
  getinfo = (retro_get_system_info)getfunc(hDLL, "retro_get_system_info");
  if (getinfo)
  {
    getinfo(&system);
    core_info entry;
    entry.fps = 60;
    entry.samplerate = 44100;
    entry.aspect_ratio = 4 / 3;

    entry.core_name = system.library_name;
    entry.core_extensions = system.valid_extensions;
    entry.core_path = path;
    cores->push_back(entry);
  }
  freelib(hDLL);
}

void CLibretro::get_cores()
{
  std::filesystem::path path = std::filesystem::current_path() / "cores";
  for (auto &entry : std::filesystem::directory_iterator(path))
  {
    string str = entry.path().string();
    if (entry.is_regular_file() && entry.path().extension() == SHLIB_EXTENSION)
      addplugin(entry.path().string().c_str(), &cores);
  }
}