#!/bin/bash

### Configuration vars

# Top level folders that might contain hardware type subfolders
TOP_DIRS_WITH_HW_SUBDIRS='files kernel libs toolchains'

# Support packages custom files patterns ($HW_TYPE - hardware type)
SUP_PKG_FILES_PAT='config-$HW_TYPE config-$HW_TYPE.h options_$HW_TYPE.h unum-$HW_TYPE.mk'


### Script code
START_DIR="$PWD"

MY_NAME=$(basename "$0")
MY_DIR=$(dirname "$0")
cd "$MY_DIR"
ROOT_DIR="$PWD"

_onexit() {
   cd "$START_DIR"
}
trap _onexit EXIT SIGHUP SIGINT SIGTERM

if [ ! -d ./src/unum ]; then
    echo "Unable to find the ./src/unum folder!"
    exit 1;
fi
HW_TYPES=`ls -1 ./src/unum/unum-*.mk | sed -e 's:\./src/unum/unum-\(.*\).mk:\1:'`

if [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
  echo "Usage: $MY_NAME [-l] <TARGET_TYPE> <SOURCE_TYPE> "
  echo "       $MY_NAME [-D] <TARGET_TYPE>"
  echo "       $MY_NAME [-X] <DIR> <TARGET_TYPE>"
  echo "TARGET_TYPE - target hardware type to create or delete"
  echo "SOURCE_TYPE - hardware type to use as the source"
  echo "-l - make lightweight (symlinks-only) hardware type"
  echo "-D - delete the target hardware type"
  echo "-X <DIR> - export the target hardware type to DIR"
  echo "-h/--help - this help"
  echo "Currently available hardware types:"
  echo "$HW_TYPES"
  exit 1
fi

# Normal hardware type (i.e. copy over, not symlink)
LWT=0
# Adding HW type (not deleting/cleaning up)
DEL=0
# external repo folder to export the hardware kind to
unset X_DIR

while [ "${1:0:1}" == "-" ]; do
case $1 in
  -l)
  echo "Lightweight (symlinked) platform"
  export LWT=1
  shift
  ;;
  -D)
  echo "Deleting hardware type"
  export DEL=1
  shift
  ;;
  -X)
  echo "Exporting hardware type"
  shift
  export X_DIR="$1"
  if [ ! -d "$X_DIR" ]; then
    echo "Error, directory $X_DIR not found!"
    exit 2
  fi
  shift
  ;;
  *)
  echo "Unrecognized option $1"
  echo "See $MY_NAME -h output for help"
  exit 2
  ;;
esac
done

T_TYPE="$1"
S_TYPE="$2"

if [ "$T_TYPE" == "" ]; then
  echo "The target hardware type cannot be empty!"
  exit 3
fi

if [ $DEL -ne 1 ] && [ "$X_DIR" == "" ]; then
  if [ "$S_TYPE" == "" ]; then
    echo "The source hardware type cannot be empty!"
    exit 4
  fi
  if [[ ! $HW_TYPES =~ (^|[[:space:]])$S_TYPE($|[[:space:]]) ]]; then
    echo "The hardware type \"$S_TYPE\" not found!"
    exit 5
  fi
else # deleting or exporting
  if [ "$S_TYPE" != "" ]; then
    echo "The source hardware type is not empty!"
    exit 6
  fi
  # set S_TYPE to be the same
  S_TYPE="$T_TYPE"
fi


# Walk through all the harware type specific folders and files

# These might have the hardware type subfolders
echo "Processing top level folders..."
for _DIR in $TOP_DIRS_WITH_HW_SUBDIRS; do
  echo "  Processing folder ./$_DIR"
  if [ "$X_DIR" != "" ]; then
    if [ -e "./$_DIR/$T_TYPE" ] || [ -L "./$_DIR/$T_TYPE" ]; then
      echo "    Exporting ./$_DIR/$T_TYPE to $X_DIR"
      mkdir -p "$X_DIR/$_DIR"
      cp -a "./$_DIR/$T_TYPE" "$X_DIR/$_DIR/$T_TYPE"
    fi
  elif [ $DEL -eq 1 ]; then
    echo "    Cleaning ./$_DIR/$T_TYPE"
    rm -Rf "./$_DIR/$T_TYPE" 2>/dev/null 1>&2
  elif [ -d "./$_DIR/$S_TYPE" ]; then
    if [ -e "./$_DIR/$T_TYPE" ]; then
      echo "    Warning, skipping ./$_DIR/$T_TYPE, already exists!"
      continue
    fi
    if [ $LWT -eq 1 ]; then
      if [ -L "./$_DIR/$T_TYPE" ]; then
        echo "    Copying symlink ./$_DIR/$S_TYPE =>./$_DIR/$T_TYPE"
        cp -a "./$_DIR/$S_TYPE" "./$_DIR/$T_TYPE"
      else
        echo "    Creating symlink ./$_DIR/$T_TYPE -> $S_TYPE"
        ln -s "$S_TYPE" "./$_DIR/$T_TYPE"
      fi
    else
      echo "    Copying folder ./$_DIR/$S_TYPE => ./$_DIR/$T_TYPE"
      cp -a "./$_DIR/$S_TYPE" "./$_DIR/$T_TYPE"
    fi
  fi
done

# These are various support packages that we compile from sources.
# Some of them contain hardware kind specifc files.
# The script has hardcoded patterns for matching those files,
# so it has to be updated if something new is added.
echo "Processing various packages under ./src..."
for _DIR in `find ./src/ -mindepth 1 -maxdepth 1 -type d`; do
  (
    echo "  Changing directory to $_DIR"
    cd "$_DIR"
    for _FILE in $SUP_PKG_FILES_PAT; do
      HW_TYPE="$S_TYPE"
      FILE=`eval "echo $_FILE"`
      HW_TYPE="$T_TYPE"
      NEW_FILE=`eval "echo $_FILE"`
      if [ ! -e "$FILE" ] && [ ! -L "$FILE" ]; then
        continue
      fi
      if [ "$X_DIR" != "" ]; then
        if [ -e "./$FILE" ] || [ -L "$FILE" ]; then
          echo "    Exporting ./$FILE to $X_DIR/$_DIR/$FILE"
          mkdir -p `dirname "$X_DIR/$_DIR/$FILE"`
          cp -a "./$FILE" "$X_DIR/$_DIR/$FILE"
        fi
      elif [ $DEL -eq 1 ]; then
        echo "    Cleaning up ./$FILE"
        rm -f "./$FILE" 2>/dev/null 1>&2
      elif [ -e "./$NEW_FILE" ]; then
        echo "    Already exists, skipping ./$NEW_FILE"
      elif [ -e "./$FILE" ]; then
        if [ $LWT -eq 1 ]; then
          if [ -L "./$FILE" ]; then
            echo "    Copying symlink ./$FILE => ./$NEW_FILE"
            cp -a "./$FILE" "./$NEW_FILE"
          else
            echo "    Creating symlink ./$NEW_FILE -> $FILE"
            ln -s "$FILE" "./$NEW_FILE"
          fi
        else
          echo "    Copying ./$FILE => ./$NEW_FILE"
          cp -a "./$FILE" "./$NEW_FILE"
        fi
      fi
    done
  )
done

# Process Unum subsystems
echo "Processing Unum subsystems..."
for _DIR in `find ./src/unum -mindepth 1 -maxdepth 1 -type d`; do
  (
    echo "  Changing directory to $_DIR"
    cd "$_DIR"
    SNAME=`basename "$_DIR"`
    MK_FILE="$SNAME-$S_TYPE.mk"
    NEW_MK_FILE="$SNAME-$T_TYPE.mk"
    DIR_NAME="$S_TYPE"
    NEW_DIR_NAME="$T_TYPE"
    # if no makefile for the hardware type then skip the subsystem
    if [ ! -e "./$MK_FILE" -a ! -L "./$MK_FILE" ] && \
       [ ! -d "./$DIR_NAME" -a ! -L "./$DIR_NAME" ]; then
      continue
    fi
    if [ "$X_DIR" != "" ]; then
      if [ -e "./$MK_FILE" ] || [ -L "./$MK_FILE" ]; then
        echo "    Exporting ./$MK_FILE to $X_DIR/$_DIR/"
        mkdir -p `dirname "$X_DIR/$_DIR/$MK_FILE"`
        cp -a "./$MK_FILE" "$X_DIR/$_DIR/$MK_FILE"
      fi
      if [ -d "$DIR_NAME" ] || [ -L "$DIR_NAME" ]; then
        echo "    Exporting folder ./$DIR_NAME to $X_DIR/$_DIR/"
        mkdir -p `dirname "$X_DIR/$_DIR/$DIR_NAME"`
        cp -a "./$DIR_NAME" "$X_DIR/$_DIR/$DIR_NAME"
      fi
    elif [ $DEL -eq 1 ]; then
      echo "    Cleaning up ./$MK_FILE"
      rm -f "./$MK_FILE" 2>/dev/null 1>&2
      if [ -e "$DIR_NAME" ] || [ -L "$DIR_NAME" ]; then
        echo "    Cleaning up folder ./$DIR_NAME"
        rm -Rf "./$DIR_NAME" 2>/dev/null 1>&2
      fi
    else
      if [ $LWT -eq 1 ]; then
        if [ -e "./$MK_FILE" ]; then
          if [ -e "./$NEW_MK_FILE" ]; then
            echo "    Already exists, skipping ./$NEW_MK_FILE"
          elif [ -L "./$MK_FILE" ]; then
            echo "    Copying symlink ./$MK_FILE => ./$NEW_MK_FILE"
            cp -a "./$MK_FILE" "./$NEW_MK_FILE"
          else
            echo "    Creating symlink ./$NEW_MK_FILE -> $MK_FILE"
            ln -s "$MK_FILE" "./$NEW_MK_FILE"
          fi
        fi
        if [ -d "./$DIR_NAME" ]; then
          if [ -e "./$NEW_DIR_NAME" ]; then
            echo "    Already exists, skipping ./$NEW_DIR_NAME"
          elif [ -L "$DIR_NAME" ]; then
            echo "    Copying symlink ./$DIR_NAME => ./$NEW_DIR_NAME"
            cp -a "./$DIR_NAME" "./$NEW_DIR_NAME"
          elif [ -d "$DIR_NAME" ]; then
            echo "    Creating symlink ./$NEW_DIR_NAME -> $DIR_NAME"
            ln -s "$DIR_NAME" "./$NEW_DIR_NAME"
          fi
        fi
      else
        if [ -e "./$MK_FILE" ]; then
          if [ -e "./$NEW_MK_FILE" ]; then
            echo "    Already exists, skipping ./$NEW_MK_FILE"
          else
            echo "    Copying file ./$MK_FILE => ./$NEW_MK_FILE"
            cp -a "./$MK_FILE" "./$NEW_MK_FILE"
          fi
        fi
        if [ -d "./$DIR_NAME" ]; then
          if [ -e "./$NEW_DIR_NAME" ]; then
            echo "    Already exists, skipping ./$NEW_DIR_NAME"
          elif [ -L "$DIR_NAME" ]; then
            echo "    Copying symlink $DIR_NAME => ./$NEW_DIR_NAME"
            cp -a "./$DIR_NAME" "./$NEW_DIR_NAME"
          elif [ -d "$DIR_NAME" ]; then
            echo "    Copying folder $DIR_NAME => ./$NEW_DIR_NAME"
            cp -a "./$DIR_NAME" "./$NEW_DIR_NAME"
          fi
        fi
      fi
    fi
  )
done

# Individual custom items
FILE="README-$T_TYPE"
if [ "$X_DIR" != "" ]; then
  if [ -e "./$FILE" ] || [ -L "./$FILE" ]; then
    echo "    Exporting ./$FILE to $X_DIR"
    cp -a "./$FILE" "$X_DIR/$FILE"
  fi
elif [ $DEL -eq 1 ]; then
  echo "Removing readme file ./$FILE"
  rm -f "./$FILE" 2>/dev/null 1>&2
else
  if [ -e "./$FILE" ]; then
    echo "Readme already exists, skipping ./$FILE"
  elif [ ! -e "$FILE" ]; then
    echo "Creating readme file ./$FILE"
    cat << READMETPL > "./$FILE"
This applies to <HARDWARE_NAME> agent build

This hardware type should be integrated with
  https://github.com/violetatrium/<REPO_NAME>
branch
  <BRANCH_NAME>

Build it under <WHERE_TO_BUILD>

Build environment setup:
------------------------
<INSTRUCTIONS_OR_WHERE_TO_GET_THEM>
READMETPL
  fi
fi

# Finally the main rules file
FILE="rules/$S_TYPE.mk"
NEW_FILE="rules/$T_TYPE.mk"
if [ "$X_DIR" != "" ]; then
  if [ -e "./$FILE" ] || [ -L "./$FILE" ]; then
    echo "    Exporting ./$FILE to $X_DIR"
    mkdir -p `dirname "$X_DIR/$FILE"`
    cp -a "./$FILE" "$X_DIR/$FILE"
  fi
elif [ $DEL -eq 1 ]; then
  echo "Removing rules file ./$FILE"
  rm -f "./$FILE" 2>/dev/null 1>&2
else
  if [ -e "./$NEW_FILE" ]; then
    echo "The rules file already exists, skipping ./$FILE"
  else
    # Rules file is always copy-only
    echo "Copying rules file ./$FILE => ./$NEW_FILE"
    cp -a "./$FILE" "./$NEW_FILE"
  fi
fi


# Remind about the extras
echo "You might also want to add/remove info under ./extras/ folder."

