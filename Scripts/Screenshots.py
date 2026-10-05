"""Renders the screenshots of the documentation from Docs/Images/Screenshots.txt.

    python Scripts/Screenshots.py                     render every image into Docs/Images
    python Scripts/Screenshots.py --only Button Menu  only images whose path contains one of these words
    python Scripts/Screenshots.py --build Build       examples from this build directory (default: Build/Release,
                                                      else Build)
    python Scripts/Screenshots.py --check             change nothing; fail if an image is out of date

An image is replaced only when it looks different: renderings on different GPUs differ by a level or two in
antialiased edges, which is not a change. That comparison needs Pillow (pip install pillow); without it, any
difference in the file counts. CI runs this script after every push to main and commits the images that changed.
"""

import argparse
import os
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGES = os.path.join(ROOT, 'Docs', 'Images')
MANIFEST = os.path.join(IMAGES, 'Screenshots.txt')

# A pixel counts as different when a channel differs by more than this; an image is out of date when more than
# this many pixels do.
CHANNEL_TOLERANCE = 16
PIXEL_TOLERANCE = 20


def read_manifest():
    entries = []
    with open(MANIFEST, encoding='utf-8') as manifest:
        for number, line in enumerate(manifest, 1):
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            words = shlex.split(line)
            if len(words) < 2:
                sys.exit(f'{MANIFEST}:{number}: expected <image> <example> [options]')
            entries.append((words[0], words[1], words[2:]))
    return entries


def find_example(build, example):
    for name in (example + '.exe', example):
        path = os.path.join(build, 'Examples', example, name)
        if os.path.isfile(path):
            return path
    sys.exit(f'{example} was not found in {build}; build the examples first')


def looks_different(new, old):
    if not os.path.isfile(old):
        return True
    try:
        from PIL import Image, ImageChops
    except ImportError:
        with open(new, 'rb') as a, open(old, 'rb') as b:
            return a.read() != b.read()
    with Image.open(new) as a, Image.open(old) as b:
        if a.size != b.size:
            return True
        difference = ImageChops.difference(a.convert('RGBA'), b.convert('RGBA'))
        # The largest channel difference of each pixel, then the pixels above the tolerance.
        channels = difference.split()
        largest = channels[0]
        for channel in channels[1:]:
            largest = ImageChops.lighter(largest, channel)
        histogram = largest.histogram()
        return sum(histogram[CHANNEL_TOLERANCE + 1:]) > PIXEL_TOLERANCE


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--build', help='build directory with the examples')
    parser.add_argument('--only', nargs='+', default=[], help='render only images whose path contains a word')
    parser.add_argument('--check', action='store_true', help='fail if an image is out of date; change nothing')
    arguments = parser.parse_args()

    build = arguments.build
    if build is None:
        build = next((os.path.join(ROOT, d) for d in (os.path.join('Build', 'Release'), 'Build')
                      if os.path.isdir(os.path.join(ROOT, d, 'Examples'))), None)
        if build is None:
            sys.exit('No build directory with examples found; pass --build')

    entries = read_manifest()
    if arguments.only:
        entries = [e for e in entries if any(word.lower() in e[0].lower() for word in arguments.only)]

    changed, failed = [], []
    with tempfile.TemporaryDirectory() as scratch:
        for image, example, options in entries:
            rendered = os.path.join(scratch, image.replace('/', '_'))
            command = [find_example(build, example), '--screenshot', rendered]
            if '--scale' not in options:
                command += ['--scale', '2']
            command += options
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
            if result.returncode != 0 or not os.path.isfile(rendered):
                failed.append(image)
                print(f'FAILED  {image}\n{result.stdout}{result.stderr}')
                continue
            target = os.path.join(IMAGES, image)
            if looks_different(rendered, target):
                changed.append(image)
                if not arguments.check:
                    os.makedirs(os.path.dirname(target), exist_ok=True)
                    shutil.copyfile(rendered, target)
                print(f'{"OUTDATED" if arguments.check else "UPDATED "} {image}')
            else:
                print(f'same     {image}')

    # Images that no entry produces are probably left over.
    listed = {os.path.normpath(e[0]) for e in read_manifest()}
    for folder, _, files in os.walk(IMAGES):
        for name in files:
            path = os.path.normpath(os.path.relpath(os.path.join(folder, name), IMAGES))
            if name.endswith('.png') and path not in listed:
                print(f'not in the manifest: {path}')

    print(f'{len(entries)} images, {len(changed)} {"out of date" if arguments.check else "updated"}, '
          f'{len(failed)} failed')
    if failed or (arguments.check and changed):
        sys.exit(1)


if __name__ == '__main__':
    main()
