"""
    MOD ROUTE python setup script
"""
import os
import platform
import shutil
from pathlib import Path
from setuptools import setup, find_packages
from setuptools.command.build_py import build_py as _build_py


# read the contents of your README file

PACKAGE_NAME = 'mod_route'

# distribution = importlib.metadata.distribution(PACKAGE_NAME)
# package_location = distribution.locate_file('')
PACKAGE_LOCATION = PACKAGE_NAME

THIS_DIRECTORY = Path(__file__).parent
LONG_DESCRIPTION = (THIS_DIRECTORY / "README.md").read_text()

LNSPDPTW_PATH = os.getenv('LNSPDPTW_PATH')

def _safe_makedirs(*paths):
    for path in paths:
        try:
            os.makedirs(path)
        except os.error:
            pass


def _get_lib_filename():
    if platform.system() == "Linux":
        lib_ext = "so"
    elif platform.system() == "Darwin":
        lib_ext = "so"
    elif platform.system() == "Windows":
        lib_ext = "dll"
    else:
        lib_ext = "so"
    return f"{PACKAGE_NAME}.{lib_ext}"


LIB_FILENAME = _get_lib_filename()


class BuildPyCommand(_build_py):
    """Custom build command."""
    def run(self):
        if platform.system() == "Windows":
            shutil.copyfile(f'../bin/{LIB_FILENAME}', f'{PACKAGE_LOCATION}/{LIB_FILENAME}')
        else:
            self.build_cmake()

        _build_py.run(self)


    def build_cmake(self):
        """Build the C library using CMake."""
        cwd = os.getcwd()

        # 디렉토리를 지우고 새로 생성
        if os.path.isdir('native'):
            shutil.rmtree('native')
        _safe_makedirs('native')

        # cmake 실행합니다
        cmake_command = ['cmake', '-S', '..', '-B', 'native',
                         '-DENABLE_PYTHON=ON',
                         '-DCMAKE_BUILD_TYPE=Release']
        if LNSPDPTW_PATH:
            cmake_command.append(f'-DLNSPDPTW_PATH={LNSPDPTW_PATH}')
        self.spawn(cmake_command)
        self.spawn(['cmake', '--build', 'native', '--target', 'mod_route'])

        # 원래 디렉토리로 돌아갑니다
        os.chdir(cwd)

        # shutil.copyfile(f'lib/build/{LIB_FILENAME}', f'{package_location}/{LIB_FILENAME}')
        shutil.copyfile(f'native/lib/{LIB_FILENAME}', f'{PACKAGE_LOCATION}/{LIB_FILENAME}')


setup(
    name="mod_route",
    version="0.9.7.3",
    description="A Python wrapper for the LNS MOD Route Library",
    long_description=LONG_DESCRIPTION,
    long_description_content_type="text/markdown",
    author=[
        { "name": "Joonkoo Kang", "email": "jkkang@ciel.co.kr" },
        { "name": "Ciel Mobility Inc.", "email": "help@ciel.co.kr" }
    ],
    classifiers=[
        "Programming Language :: Python :: 3",
        "Operating System :: OS Independent",
    ],
    license="MIT",
    package_dir={"": "."},
    packages=find_packages(),
    python_requires=">=3.8",
    cmdclass={
        "build_py": BuildPyCommand,
    },
    package_data={
        "": ["mod_route.*"],
    },
)
