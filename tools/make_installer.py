"""Build the per-user installer from the verified portable distribution."""
from pathlib import Path
import re
import subprocess
ROOT = Path(__file__).resolve().parents[1]
version = re.search(r'project\(ShaderixFrame VERSION ([\d.]+)',(ROOT/'CMakeLists.txt').read_text('utf-8')).group(1)
compiler = Path('C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe')
archive = ROOT/f'dist/ShaderixFrame-{version}-windows-x64.zip'
output = ROOT/f'dist/ShaderixFrame-{version}-Setup.exe'
subprocess.run([str(compiler),'/nologo','/target:winexe','/platform:x64',f'/out:{output}',
               '/reference:System.IO.Compression.dll','/reference:System.IO.Compression.FileSystem.dll',
               '/reference:System.Windows.Forms.dll','/reference:Microsoft.CSharp.dll',
               '/reference:System.Drawing.dll',
               f'/resource:{archive},portable.zip',str(ROOT/'tools/Installer.cs')],check=True)
print(output)
