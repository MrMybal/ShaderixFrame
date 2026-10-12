using System;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Windows.Forms;

class Installer {
    static string Hash(string path) {using(var sha=SHA256.Create())using(var file=File.OpenRead(path))return BitConverter.ToString(sha.ComputeHash(file)).Replace("-","");}
    static string Within(string root,string relative) {
        if(Path.IsPathRooted(relative))throw new Exception("Chemin absolu interdit dans l'archive");
        var path=Path.GetFullPath(Path.Combine(root,relative));
        if(!path.StartsWith(root+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))throw new Exception("Chemin hors du dossier d'installation");
        for(var parent=Path.GetDirectoryName(path);parent!=null && parent.StartsWith(root,StringComparison.OrdinalIgnoreCase);parent=Path.GetDirectoryName(parent))
            if(Directory.Exists(parent) && (File.GetAttributes(parent)&FileAttributes.ReparsePoint)!=0)throw new Exception("Dossier de redirection non accepté");
        return path;
    }
    static void Install(string directory,bool shortcuts) {
        var root=Path.GetFullPath(directory).TrimEnd(Path.DirectorySeparatorChar);
        if(root==Path.GetPathRoot(root).TrimEnd(Path.DirectorySeparatorChar))throw new Exception("Choisir un dossier dédié à ShaderixFrame");
        Directory.CreateDirectory(root);
        var manifest=new List<string>();
        using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("portable.zip"))
        using(var archive=new ZipArchive(stream,ZipArchiveMode.Read)) {
            foreach(var entry in archive.Entries) {
                if(!entry.FullName.StartsWith("ShaderixFrame/",StringComparison.Ordinal))throw new Exception("Racine d'archive incorrecte");
                var relative=entry.FullName.Substring("ShaderixFrame/".Length).Replace('/',Path.DirectorySeparatorChar);
                if(relative.Length==0 || entry.Name.Length==0)continue;
                var target=Within(root,relative);
                Directory.CreateDirectory(Path.GetDirectoryName(target));
                var pending=target+".install-"+Guid.NewGuid().ToString("N");
                try {
                    using(var source=entry.Open())using(var output=File.Create(pending))source.CopyTo(output);
                    if(File.Exists(target))File.Replace(pending,target,null);else File.Move(pending,target);
                }finally{if(File.Exists(pending))File.Delete(pending);}
                manifest.Add(Hash(target)+"\t"+relative);
            }
        }
        File.WriteAllLines(Path.Combine(root,"installed-files.txt"),manifest);
        if(shortcuts) {
            var type=Type.GetTypeFromProgID("WScript.Shell");dynamic shell=Activator.CreateInstance(type);
            dynamic shortcut=shell.CreateShortcut(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),"ShaderixFrame.lnk"));
            shortcut.TargetPath=Path.Combine(root,"ShaderixFrame.exe");shortcut.WorkingDirectory=root;shortcut.Save();
            System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shortcut);System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shell);
        }
    }
    static void Uninstall(string directory) {
        var root=Path.GetFullPath(directory).TrimEnd(Path.DirectorySeparatorChar);
        var manifest=Path.Combine(root,"installed-files.txt");
        if(!File.Exists(manifest))throw new Exception("Manifeste d'installation introuvable");
        var shortcutPath=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),"ShaderixFrame.lnk");
        if(File.Exists(shortcutPath)) {
            var type=Type.GetTypeFromProgID("WScript.Shell");dynamic shell=Activator.CreateInstance(type);
            dynamic shortcut=shell.CreateShortcut(shortcutPath);
            string target=shortcut.TargetPath;
            System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shortcut);System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shell);
            if(String.Equals(target,Path.Combine(root,"ShaderixFrame.exe"),StringComparison.OrdinalIgnoreCase))File.Delete(shortcutPath);
        }
        var directories=new HashSet<string>();
        foreach(var line in File.ReadAllLines(manifest)) {
            var parts=line.Split('\t');if(parts.Length!=2)throw new Exception("Manifeste invalide");
            var file=Within(root,parts[1]);
            // Edited files, projects, recoveries, caches and files outside the manifest are retained.
            if(File.Exists(file) && Hash(file)==parts[0])File.Delete(file);
            for(var parent=Path.GetDirectoryName(file);parent!=root;parent=Path.GetDirectoryName(parent))directories.Add(parent);
        }
        File.Delete(manifest);
        var sorted=new List<string>(directories);sorted.Sort((a,b)=>b.Length.CompareTo(a.Length));
        foreach(var folder in sorted)if(Directory.Exists(folder) && Directory.GetFileSystemEntries(folder).Length==0)Directory.Delete(folder);
        if(Directory.Exists(root) && Directory.GetFileSystemEntries(root).Length==0)Directory.Delete(root);
    }
    [STAThread] static int Main(string[] args) {
        bool silent=Array.IndexOf(args,"--silent")>=0,shortcuts=Array.IndexOf(args,"--no-shortcuts")<0;
        string directory=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Programs","ShaderixFrame");
        bool uninstall=false;
        for(int i=0;i<args.Length;i++)if((args[i]=="--install-dir" || args[i]=="--uninstall") && i+1<args.Length){uninstall=args[i]=="--uninstall";directory=args[++i];}
        try {
            if(!silent) {
                Application.EnableVisualStyles();
                using(var form=new Form()){form.Text=uninstall?"Désinstaller ShaderixFrame":"Installer ShaderixFrame";form.Width=600;form.Height=180;form.FormBorderStyle=FormBorderStyle.FixedDialog;form.MaximizeBox=false;
                    var label=new Label{Left=16,Top=15,Width=550,Height=25,Text=uninstall?"Les projets et fichiers modifiés seront conservés.":"Installation pour votre compte Windows :"};
                    var path=new TextBox{Left=16,Top=45,Width=550,Text=directory};
                    var accept=new Button{Left=420,Top=85,Width=145,Text=uninstall?"Désinstaller":"Installer",DialogResult=DialogResult.OK};
                    form.Controls.AddRange(new Control[]{label,path,accept});form.AcceptButton=accept;
                    if(form.ShowDialog()!=DialogResult.OK)return 0;directory=path.Text;}
            }
            if(uninstall)Uninstall(directory);else Install(directory,shortcuts);
            if(!silent)MessageBox.Show(uninstall?"Désinstallation terminée.":"Installation terminée.","ShaderixFrame");return 0;
        }catch(Exception error){if(silent)Console.Error.WriteLine(error.Message);else MessageBox.Show(error.Message,"ShaderixFrame",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;}
    }
}
