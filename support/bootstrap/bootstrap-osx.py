#!/usr/bin/env python
# Set up build environment on OS X Moutain Lion.

from __future__ import print_function
from bootstrap import *
import glob, os, tempfile
from subprocess import call, check_call

vagrant = bootstrap_init()

if vagrant:
  # Disable the screensaver because it breaks GUI tests and consumes resources.
  check_call(['sudo', '-u', 'vagrant', 'defaults', 'write',
              'com.apple.screensaver', 'idleTime', '0'])
  # Kill the screensaver if it has started already.
  call(['killall', 'ScreenSaverEngine'])

install_cmake('cmake-3.1.0-Darwin64-universal.tar.gz')
install_maven()

# Install command-line tools for Xcode.
if not installed('clang'):
  with download(
      'http://devimages.apple.com/downloads/xcode/' +
      'command_line_tools_for_xcode_os_x_mountain_lion_april_2013.dmg') as f:
    install_dmg(f, True)

# Install MacPorts.
if not installed('port'):
  with download(
      'https://distfiles.macports.org/MacPorts/' +
      'MacPorts-2.2.0-10.8-MountainLion.pkg') as f:
    install_pkg(f)
  # Get rid of "No Xcode installation was found" error.
  macports_conf = '/opt/local/etc/macports/macports.conf'
  macports_conf_new = macports_conf + '.new'
  if os.path.exists(macports_conf):
    with open(macports_conf, 'r') as f:
      conf = f.read()
    with open(macports_conf_new, 'w') as f:
      f.write(re.sub(r'#\s*(developer_dir\s*).*', r'\1/', conf))
    os.remove(macports_conf)
  # The new config may also exists if the script was interrupted just after
  # removing the old config and then restarted.
  if os.path.exists(macports_conf_new):
    os.rename(macports_conf_new, macports_conf)
  add_to_path('/opt/local/bin/port')

# Install ccache.
if not installed('ccache'):
  check_call(['port', 'install', 'ccache'])
  home = os.path.expanduser('~vagrant' if vagrant else '~')
  with open(home + '/.profile', 'a') as f:
    f.write('export PATH=/opt/local/libexec/ccache:$PATH')
  add_to_path('/opt/local/libexec/ccache', None, True)

# Install gfortran.
if not installed('gfortran-4.9'):
  check_call(['port', 'install', 'gcc49', '+gfortran'])
  add_to_path('/opt/local/bin/gfortran-mp-4.9', 'gfortran-4.9')

install_f90cache()
create_symlink('/usr/local/bin/f90cache',
               '/opt/local/libexec/ccache/gfortran-4.9')

# Install JDK.
with download(
    'http://download.oracle.com/otn-pub/java/jdk/7u79-b15/' +
    'jdk-7u79-macosx-x64.dmg', jdk_cookie) as f:
  install_dmg(f)

# Install LocalSolver.
if not installed('localsolver'):
  with download('http://www.localsolver.com/downloads/' +
      'LocalSolver_{0}_MacOS64.pkg'.format(LOCALSOLVER_VERSION)) as f:
    install_pkg(f)

copy_optional_dependencies('osx', '/mnt')
for dir in ['Xcode.app', 'MATLAB_R2014a.app']:
  if os.path.exists('/opt/' + dir):
    create_symlink('/opt/' + dir, '/Applications/' + dir)

check_call([vagrant_dir + '/support/bootstrap/accept-xcode-license'])

buildbot_path = install_buildbot_slave('osx-ml')
if buildbot_path:
  # Add buildslave launch agent.
  plist_name = 'buildslave.plist'
  with open(vagrant_dir + '/support/buildbot/' + plist_name, 'r') as f:
    plist_content = f.read()
  plist_content = plist_content.replace('$PATH', os.environ['PATH'])
  launchagents_dir = '/Users/vagrant/Library/LaunchAgents'
  try:
    os.makedirs(launchagents_dir)
  except OSError as e:
    if not (e.errno == errno.EEXIST and os.path.isdir(launchagents_dir)):
      raise
  plist_path = os.path.join(launchagents_dir, plist_name)
  with open(plist_path, 'w') as f:
    f.write(plist_content)
  check_call(['launchctl', 'load', plist_path])
