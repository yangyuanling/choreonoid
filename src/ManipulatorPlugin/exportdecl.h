#ifndef CNOID_MANIPULATORPLUGIN_EXPORTDECL_H
# define CNOID_MANIPULATORPLUGIN_EXPORTDECL_H
# if defined _WIN32 || defined __CYGWIN__
#  define CNOID_MANIPULATORPLUGIN_DLLIMPORT __declspec(dllimport)
#  define CNOID_MANIPULATORPLUGIN_DLLEXPORT __declspec(dllexport)
#  define CNOID_MANIPULATORPLUGIN_DLLLOCAL
# else
#  if __GNUC__ >= 4
#   define CNOID_MANIPULATORPLUGIN_DLLIMPORT __attribute__ ((visibility("default")))
#   define CNOID_MANIPULATORPLUGIN_DLLEXPORT __attribute__ ((visibility("default")))
#   define CNOID_MANIPULATORPLUGIN_DLLLOCAL  __attribute__ ((visibility("hidden")))
#  else
#   define CNOID_MANIPULATORPLUGIN_DLLIMPORT
#   define CNOID_MANIPULATORPLUGIN_DLLEXPORT
#   define CNOID_MANIPULATORPLUGIN_DLLLOCAL
#  endif
# endif

# ifdef CNOID_MANIPULATORPLUGIN_STATIC
#  define CNOID_MANIPULATORPLUGIN_DLLAPI
#  define CNOID_MANIPULATORPLUGIN_LOCAL
# else
#  ifdef CnoidManipulatorPlugin_EXPORTS
#   define CNOID_MANIPULATORPLUGIN_DLLAPI CNOID_MANIPULATORPLUGIN_DLLEXPORT
#  else
#   define CNOID_MANIPULATORPLUGIN_DLLAPI CNOID_MANIPULATORPLUGIN_DLLIMPORT
#  endif
#  define CNOID_MANIPULATORPLUGIN_LOCAL CNOID_MANIPULATORPLUGIN_DLLLOCAL
# endif

#endif

#ifdef CNOID_EXPORT
# undef CNOID_EXPORT
#endif
#define CNOID_EXPORT CNOID_MANIPULATORPLUGIN_DLLAPI
