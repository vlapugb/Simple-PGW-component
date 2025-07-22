import subprocess
import os
import sys

projdir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
os.makedirs(f"{projdir}/build", exist_ok=True)
os.chdir(f"{projdir}/build")
build_type = sys.argv[1] if len(sys.argv) > 1 else "Debug"
subprocess.run(["cmake", "-G", "Ninja",
                f"-DCMAKE_BUILD_TYPE={build_type}",
                "-DCMAKE_C_COMPILER_LAUNCHER=ccache",
                "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache",
                ".."], check=True)
subprocess.run(["cmake", "--build", ".", "--parallel"], check=True)
subprocess.run(["ctest", "--output-on-failure"], check=True)
