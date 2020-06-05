Vagrant.configure('2') do |config|
  config.vm.box = 'ubuntu/xenial64'
  config.vm.provision 'shell', path: 'vagrant/provision.sh'
  config.vm.synced_folder '../', '/repos'
end
