# River Precompiled Binaries

For simplicity for the user & developer of the plugin, we bundle the precompiled River (and other necessary) libraries into the plugin itself. River, more generally, is built and distributed through Conda, a cross-platform package manager, and what the infrastructure in this directory does is download the necessary binaries and includes from Conda to make them available locally.

A bash script is provided here in the `scripts` folder that supports downloading the packages for each different architecture. To update these do:

```
bash scripts/update_river.sh
```

which should entirely refresh the various folders in this `Resources` parent directory. Doing this directly via conda guarantees compatibility across all dependencies.

To upgrade the River version, change the `PACKAGE_VERSION` variable at the top of the `scripts/update_river.sh` script.
