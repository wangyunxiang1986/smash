# Docker containers

Information about Docker can be found on the [Docker official page](https://docs.docker.com/).
The links to download Docker for Linux, Mac and Windows10 are also [officially available](https://www.docker.com/get-started).
The instructions to build SMASH are written in the file _Dockerfile_ and, apart from the different syntax, the procedure is the same as in the case of Singularity (see [following section](#docker-to-singularity)).
As explained in the [SMASH README file](../README.md), Docker images are provided as [packages in the Github organisation](https://github.com/orgs/smash-transport/packages) and can be pulled from there and directly run. 

### Building a Docker image for SMASH

Assuming that Docker is already installed, one can also build images locally by executing
```
docker buildx build -f Dockerfile .
```
in a terminal, in the same directory of _Dockerfile_.

Docker keeps the information about the container in a directory (e.g. in ***/var/lib/docker/*** under GNU/Linux), instead of just in a file like Singularity.
One can get an overview of the current Docker containers/installation with `docker info` and, in particular, it is possible to list the available images with `docker images`.
The images have each its own id.
One can recognize the last one just produced by the creation date (tags are also possible, but not discussed here).
Run a container with `docker run -it <image_id>`.
You might need to use `sudo` in front of the docker commands.
To export an image into a file, use `docker save -o name_of_Docker_container_image.tar image_id`.
It is suggested to compress the file (e.g. with gzip) before uploading it into the cluster or somewhere else.
Once the tar archive of the Docker container is in the cluster, one can transform it into a singularity container via
```
singularity build name_of_the_new_singularity_container.sif docker-archive://name_of_Docker_container_image.tar
```

We recall that, to change the default prompt of the bash shell, the user can set up the environment variable `PS1` (refer to [the bash manual](https://www.gnu.org/software/bash/manual/bash.html#Controlling-the-Prompt) for more information).

Note that default resources allocated for the Docker deamon might not be enough. In particular, the virtual memory might be insufficent and lead to crashes.
Try increasing those resoruces, if the build process gets suddenly terminated.

### Using a Docker container for development

If you want to use the container environment for development, it is useful to mirror a smash development repository into a container directory with running the container with `-v` option as follows:
```
docker run -it -v path/to/smash/repo:/SMASH/smash_local  <image_id or tag>
```
This creates the directory `/SMASH/smash_local` which matches the smash directory.
All local changes will be reflected in this directory and SMASH can be build with those changes in the container.


<a id="docker-to-singularity"></a>

# From Docker to Singularity/Apptainer

We remind that in many HPC systems Singularity is actually replaced by Apptainer, but the two projects have tight contacts and an extremely high compatibility.
In the examples we will refer only to Singularity, but the commands are exactly the same also with Apptainer, including the name of the executable `singularity`.
In most cases Singularity and Apptainer are able to import or directly run commands from Docker containers without significant problems.
In both situations Singularity and Apptainer cache information in a subdirectory of ***~/.singularity*** or ***~/.apptainer***, respectively.
More information can be found [here](https://docs.sylabs.io/guides/latest/user-guide/singularity_and_docker.html) and [here](https://apptainer.org/docs/user/latest/docker_and_oci.html).

### Import a Docker image from an online registry

It is possible to retrieve a Docker image _docker-image_ from an online registry `ghcr.io/repositoryname/` and tranform it into a Singularity image, named in this case _my-singularity-image.sif_, with:
```
singularity pull my-singularity-image.sif docker://ghcr.io/repositoryname/docker-image
```

For example, in the case of the official SMASH basic image:
```
singularity pull smash.sif docker://ghcr.io/smash-transport/smash:newest
```

It is possible also to execute single commands in the container.
For example
```
singularity exec docker://ghcr.io/repositoryname/docker-image bash
```
launches a bash shell within the container.
If the Docker image has been already cached, Singularity uses the local copy in `.sif` format, otherwise it downloads the Docker image and converts it into `.sif` format in the internal cache without creating another additional `.sif` file.
Then, of course, Singularity executes the command.


### Import a local Docker image

First, the local Docker image should be exported as a tar archive.
For example, assuming that we want to export the image _abcd1234_, use
```
docker save --output=abcd1234-docker-cont.tar abcd1234
```

In general it is convenient to compress the tar archive before transmission over internet and decompress it before conversion.

To convert _abcd1234-docker-cont.tar_ into _mycontainer-image.sif_ (both in the same current working directory) use
```
singularity build mycontainer-image.sif docker-archive://abcd1234.tar
```

This kind of operation does not require root privileges.
