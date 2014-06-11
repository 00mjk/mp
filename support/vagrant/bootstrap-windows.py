# Set up build environment on 64-bit Windows.

from __future__ import print_function
import os, shutil
from bootstrap import *
from glob import glob
from subprocess import check_call

# Add Python to PATH.
python_dir = r'C:\Python27'
if os.path.exists(python_dir) and not installed('python'):
  add_to_path(python_dir + r'\python')

install_cmake('cmake-2.8.12.2-win32-x86.zip')

# Install .NET Framework 4 for msbuild.
if not os.path.exists(r'\Windows\Microsoft.NET\Framework64\v4.0.30319'):
  with download(
      'http://download.microsoft.com/download/9/5/A/' +
      '95A9616B-7A37-4AF6-BC36-D6EA96C8DAAE/dotNetFx40_Full_x86_x64.exe') as f:
    check_call([f, '/q', '/norestart'])

# Install 7zip.
sevenzip = r'C:\Program Files (x86)\7-Zip\7z.exe'
if not os.path.exists(sevenzip):
  with download('http://downloads.sourceforge.net/sevenzip/7z920.exe') as f:
    check_call([f, '/S'])

# Install Windows SDK.
if not os.path.exists(r'\Program Files\Microsoft SDKs\Windows\v7.1'):
  # Extract ISO.
  with download(
       'http://download.microsoft.com/download/F/1/0/'
       'F10113F5-B750-4969-A255-274341AC6BCE/GRMSDKX_EN_DVD.iso') as f:
    check_call([sevenzip, 'x', '-tudf', '-owinsdk', f])
  # Install SDK.
  check_call([r'winsdk\setup.exe', '-q'])
  shutil.rmtree('winsdk')

# Install MinGW.
def install_mingw(arch):
  bits = '64' if arch.endswith('64') else '32'
  if os.path.exists(r'\mingw' + bits):
    return
  with download(
      'http://sourceforge.net/projects/mingw-w64/files/' +
      'Toolchains%20targetting%20Win' + bits + '/Personal%20Builds/' +
      'mingw-builds/4.8.2/threads-win32/sjlj/' + arch +
      '-4.8.2-release-win32-sjlj-rt_v3-rev4.7z/download') as f:
    check_call([sevenzip, 'x', '-oC:\\', f])

install_mingw('i686')
install_mingw('x86_64')

# Install 32-bit JDK.
cookie = 'oraclelicense=accept-securebackup-cookie'
if not os.path.exists(r'\Program Files (x86)\Java\jdk1.7.0_55'):
  with download(
      'http://download.oracle.com/otn-pub/java/jdk/7u55-b13/' +
      'jdk-7u55-windows-i586.exe', cookie) as f:
    check_call([f, '/s'])

# Install 64-bit JDK.
if not os.path.exists(r'\Program Files\Java\jdk1.7.0_55'):
  with download(
      'http://download.oracle.com/otn-pub/java/jdk/7u55-b13/' +
      'jdk-7u55-windows-x64.exe', cookie) as f:
    check_call([f, '/s'])

# Copy optional dependencies.
opt_dir = r'opt\win64'
if os.path.exists(opt_dir):
  for entry in os.listdir(opt_dir):
    subdir = os.path.join(opt_dir, entry)
    for subentry in os.listdir(subdir):
      dest = os.path.join('C:\\', entry, subentry)
      if not os.path.exists(dest):
        shutil.copytree(os.path.join(subdir, subentry), dest)

# Install pywin32 - buildbot dependency.
if not module_exists('win32api'):
  shutil.rmtree('pywin32', True)
  with download(
      'http://sourceforge.net/projects/pywin32/files/pywin32/Build%20219/' +
      'pywin32-219.win-amd64-py2.7.exe/download') as f:
    check_call([sevenzip, 'x', '-opywin32', f])
  site_packages_dir = os.path.join(python_dir, r'lib\site-packages')
  for path in glob('pywin32/PLATLIB/*') + glob('pywin32/SCRIPTS/*'):
    shutil.move(path, site_packages_dir)
  shutil.rmtree('pywin32')
  import pywin32_postinstall
  pywin32_postinstall.install()
  os.remove(site_packages_dir + r'\pywin32_postinstall.py')

# Specifies whether to restart BuildBot service.
restart = False

# Remove sh.exe from the path as it breaks MinGW makefile generation.
paths = os.environ['PATH'].split(os.pathsep)
for path in paths:
  if os.path.exists(os.path.join(path.strip('"'), "sh.exe")):
    print('Removing', path, 'from $PATH')
    paths.remove(path)
    restart = True
os.environ['PATH'] = os.pathsep.join(paths)
check_call(['setx', '/m', 'PATH', os.environ['PATH']])

import win32serviceutil
import _winreg as reg

buildslave_dir = r'\buildslave'
if not os.path.exists(buildslave_dir):
  install_buildbot_slave('win2008', buildslave_dir, python_dir + r'\Scripts', True)

  # Grant the user the right to "log on as a service".
  import win32api, win32security
  username = win32api.GetUserNameEx(win32api.NameSamCompatible)
  domain, username = username.split('\\')
  policy = win32security.LsaOpenPolicy(domain, win32security.POLICY_ALL_ACCESS)
  sid_obj, domain, tmp = win32security.LookupAccountName(domain, username)
  win32security.LsaAddAccountRights(policy, sid_obj, ('SeServiceLogonRight',))
  win32security.LsaClose(policy)

  # Set buildslave parameters.
  with reg.CreateKey(reg.HKEY_LOCAL_MACHINE,
      r'System\CurrentControlSet\services\BuildBot\Parameters') as key:
    reg.SetValueEx(key, 'directories', 0, reg.REG_SZ, buildslave_dir)

  # Install buildbot service.
  check_call([
    os.path.join(python_dir, 'python'),
    os.path.join(python_dir, r'Scripts\buildbot_service.py'),
    '--user', '.\\' + username, '--password', 'vagrant',
    '--startup', 'auto', 'install'])
  win32serviceutil.StartService('BuildBot')
elif restart:
  print('Restarting BuildBot')
  win32serviceutil.RestartService('BuildBot')

# Add a registry key value to get rid of SetEnv.cmd errors
# "The system was unable to find the specified registry key or value."
# that are causing custom builds to fail.
key_name = r'SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VS7'
with reg.CreateKey(reg.HKEY_LOCAL_MACHINE, key_name) as key:
  value_name = '10.0'
  try:
    reg.QueryValueEx(key, value_name)
  except:
    print('Adding value', value_name, 'for registry key', key_name)
    reg.SetValueEx(key, value_name, 0, reg.REG_SZ, '')
