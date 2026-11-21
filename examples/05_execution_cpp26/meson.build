project('io-uring-execution-experiment', 'cpp',
  default_options : ['cpp_std=c++26', 'warning_level=3'])

stdexec_dep = subproject('stdexec').get_variable('stdexec_dep')
threads = dependency('threads')

execution_smoke = executable('execution_smoke', 'execution_smoke.cpp',
  dependencies : [stdexec_dep, threads])
test('execution lifecycle', execution_smoke)

liburing = dependency('liburing')
read_smoke = executable('read_smoke', 'read_smoke.cpp',
  dependencies : [stdexec_dep, liburing])
test('single-shot file read', read_smoke)
