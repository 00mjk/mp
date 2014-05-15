# -*- mode: ruby -*-
# vi: set ft=ruby :

# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  # This requires VirtualBox Extension Pack to be installed on the host.
  config.vm.provider "virtualbox" do |v|
    v.customize ["modifyvm", :id, "--vrde", "on", "--vrdeauthtype", "external"]
  end

  config.vm.define "lucid32" do |c|
    c.vm.box = "lucid32"
    c.vm.box_url = "http://files.vagrantup.com/lucid32.box"
    c.vm.synced_folder "scripts/vagrant/lucid32/archives", "/var/cache/apt/archives"
    c.vm.provision :shell, :path => "scripts/bootstrap-ubuntu.sh"
  end

  config.vm.define "lucid64", primary: true do |c|
    c.vm.box = "lucid64"
    c.vm.box_url = "http://files.vagrantup.com/lucid64.box"
    c.vm.synced_folder "scripts/vagrant/lucid64/archives", "/var/cache/apt/archives"
    c.vm.provision :shell, :path => "scripts/bootstrap-ubuntu.sh"
  end

  config.vm.define "osx-mavericks" do |c|
    c.vm.box = "osx-mavericks"
    c.vm.network :private_network, ip: "10.11.12.13"
    c.vm.synced_folder ".", "/vagrant", :type => "nfs", :mount_options => ["resvport"]
  end

  config.vm.define "win2008" do |c|
    c.vm.box = "win2008"
    c.vm.guest = :windows
    c.vm.communicator = "winrm"
  end
end
