import subprocess
import os
import sys


BASE_DIR = os.path.dirname(os.path.abspath(__file__))
server_settings_path = os.path.abspath(os.path.join(BASE_DIR, '../settings/server_settings.json'))
client_settings_path = os.path.abspath(os.path.join(BASE_DIR, '../settings/client_settings.json'))
set_env_path = os.path.abspath(os.path.join(BASE_DIR, 'set_env.sh'))

with open(set_env_path, 'w', encoding='utf-8') as f:
    f.write(f'export SERVER_SETTINGS="{server_settings_path}"\n')
    f.write(f'export CLIENT_SETTINGS="{client_settings_path}"\n')
projdir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
os.makedirs(f"{projdir}/build", exist_ok=True)
os.chdir(f"{projdir}/build")
build_type = sys.argv[1] if len(sys.argv) > 1 else "Debug"
subprocess.run(["cmake", "-G", "Ninja",
                f"-DCMAKE_BUILD_TYPE={build_type}",
                ".."], check=True)
subprocess.run(["cmake", "--build", ".", "--parallel"], check=True)
subprocess.run(["ctest", "--output-on-failure"], check=True)