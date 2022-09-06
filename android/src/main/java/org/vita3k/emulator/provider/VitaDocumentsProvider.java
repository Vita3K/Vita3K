package org.vita3k.emulator.provider;

import android.database.Cursor;
import android.database.MatrixCursor;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsProvider;
import android.util.Log;
import android.webkit.MimeTypeMap;

import androidx.annotation.Nullable;

import org.vita3k.emulator.R;

import java.io.File;
import java.io.FileNotFoundException;

// Strongly inspired by https://github.com/libretro/RetroArch/blob/master/pkg/android/phoenix/src/com/retroarch/browser/provider/RetroDocumentsProvider.java
public class VitaDocumentsProvider extends DocumentsProvider {

    private final String root = "VitaRoot";

    private final String[] DEFAULT_ROOT_PROJECTION = new String[]{
            DocumentsContract.Root.COLUMN_ROOT_ID,
            DocumentsContract.Root.COLUMN_MIME_TYPES,
            DocumentsContract.Root.COLUMN_FLAGS,
            DocumentsContract.Root.COLUMN_TITLE,
            DocumentsContract.Root.COLUMN_DOCUMENT_ID,
            DocumentsContract.Root.COLUMN_AVAILABLE_BYTES,
            DocumentsContract.Root.COLUMN_ICON
    };

    private final String[] DEFAULT_DOCUMENT_PROJECTION = new String[]{
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_LAST_MODIFIED,
            DocumentsContract.Document.COLUMN_FLAGS,
            DocumentsContract.Document.COLUMN_SIZE,
            DocumentsContract.Document.COLUMN_ICON
    };

    @Override
    public boolean onCreate() {
        return true;
    }

    private static String getMimeType(File file) {
        if (file.isDirectory()) {
            return DocumentsContract.Document.MIME_TYPE_DIR;
        } else {
            final String name = file.getName();
            final int lastDot = name.lastIndexOf('.');
            if (lastDot >= 0) {
                final String extension = name.substring(lastDot + 1).toLowerCase();
                final String mime = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
                if (mime != null) return mime;
            }
            return "application/octet-stream";
        }
    }

    private File getStorageDir(){
        File storage_dir = new File(getContext().getExternalFilesDir(null), "vita");
        if(!storage_dir.exists())
            storage_dir.mkdirs();

        return storage_dir;
    }

    private File resolveFile(String documentId) throws FileNotFoundException {
        if(documentId.startsWith(root)){
            File storage_dir = getStorageDir();

            storage_dir = new File(storage_dir, documentId.substring(root.length() + 1));
            if(!storage_dir.exists())
                throw new FileNotFoundException(documentId + " was not found");

            return storage_dir;
        }

        throw new FileNotFoundException(documentId + " was not found");
    }

    private String getDocumentId(File file) {
        return root + ":" + file.getAbsolutePath().substring(getStorageDir().getAbsolutePath().length());
    }

    private void applyCursor(MatrixCursor cursor, File file, String documentId) throws FileNotFoundException {
        if(file == null)
            file = resolveFile(documentId);
        if(documentId == null)
            documentId = getDocumentId(file);

        boolean is_root = file == getStorageDir();
        int flags = 0;
        if(file.isDirectory()){
            flags = DocumentsContract.Document.FLAG_DIR_SUPPORTS_CREATE
                    | DocumentsContract.Document.FLAG_SUPPORTS_DELETE
                    | DocumentsContract.Document.FLAG_SUPPORTS_REMOVE
                    | DocumentsContract.Document.FLAG_SUPPORTS_RENAME;
        } else {
            flags = DocumentsContract.Document.FLAG_SUPPORTS_COPY
                    | DocumentsContract.Document.FLAG_SUPPORTS_DELETE
                    | DocumentsContract.Document.FLAG_SUPPORTS_MOVE
                    | DocumentsContract.Document.FLAG_SUPPORTS_REMOVE
                    | DocumentsContract.Document.FLAG_SUPPORTS_RENAME
                    | DocumentsContract.Document.FLAG_SUPPORTS_WRITE;
        }

        MatrixCursor.RowBuilder row = cursor.newRow();
        row.add(DocumentsContract.Document.COLUMN_DOCUMENT_ID, documentId)
                .add(DocumentsContract.Document.COLUMN_DISPLAY_NAME, is_root ? "Vita3K" : file.getName())
                .add(DocumentsContract.Document.COLUMN_SIZE, file.length())
                .add(DocumentsContract.Document.COLUMN_MIME_TYPE, getMimeType(file))
                .add(DocumentsContract.Document.COLUMN_LAST_MODIFIED, file.lastModified())
                .add(DocumentsContract.Document.COLUMN_FLAGS, flags);
        if(is_root)
            row.add(DocumentsContract.Document.COLUMN_ICON, R.mipmap.ic_launcher);
    }

    @Override
    public Cursor queryRoots(String[] projection) throws FileNotFoundException {
        MatrixCursor cursor = new MatrixCursor(projection == null ? DEFAULT_ROOT_PROJECTION : projection);
        File storage_dir = getStorageDir();

        cursor.newRow()
                .add(DocumentsContract.Root.COLUMN_ROOT_ID, root)
                .add(DocumentsContract.Root.COLUMN_FLAGS, DocumentsContract.Root.FLAG_SUPPORTS_CREATE | DocumentsContract.Root.FLAG_SUPPORTS_IS_CHILD)
                .add(DocumentsContract.Root.COLUMN_TITLE, "Vita3K")
                .add(DocumentsContract.Root.COLUMN_DOCUMENT_ID, getDocumentId(storage_dir))
                .add(DocumentsContract.Root.COLUMN_MIME_TYPES, "*/*")
                .add(DocumentsContract.Root.COLUMN_AVAILABLE_BYTES, storage_dir.getFreeSpace())
                .add(DocumentsContract.Root.COLUMN_ICON, R.mipmap.ic_launcher);

        return cursor;
    }

    @Override
    public Cursor queryDocument(String documentId, String[] projection) throws FileNotFoundException {
        MatrixCursor cursor = new MatrixCursor(projection == null ? DEFAULT_DOCUMENT_PROJECTION : projection);
        applyCursor(cursor, null, documentId);
        return cursor;
    }

    @Override
    public Cursor queryChildDocuments(String parentDocumentId, String[] projection, String sortOrder) throws FileNotFoundException {
        MatrixCursor cursor = new MatrixCursor(projection == null ? DEFAULT_DOCUMENT_PROJECTION : projection);
        File folder = resolveFile(parentDocumentId);
        for(File child : folder.listFiles())
            applyCursor(cursor, child, null);
        return cursor;
    }

    @Override
    public ParcelFileDescriptor openDocument(String documentId, String mode, @Nullable CancellationSignal signal) throws FileNotFoundException {
        File file = resolveFile(documentId);
        int access_mode = ParcelFileDescriptor.parseMode(mode);
        return ParcelFileDescriptor.open(file, access_mode);
    }

    @Override
    public String createDocument(String parentDocumentId, String mimeType, String displayName) throws FileNotFoundException {
        File parent = resolveFile(parentDocumentId);
        int nb_conflicts = 1;
        File new_file = new File(parent, displayName);
        while(new_file.exists())
            new_file = new File(parent, displayName + " (" + (++nb_conflicts) + ")");

        try {
            boolean success = false;
            if (mimeType.equals(DocumentsContract.Document.MIME_TYPE_DIR))
                success = new_file.mkdir();
            else
                success = new_file.createNewFile();

            if(!success)
                throw new FileNotFoundException();
        } catch (Exception e){
            throw new FileNotFoundException();
        }

        return getDocumentId(new_file);
    }

    @Override
    public void deleteDocument(String documentId) throws FileNotFoundException {
        File file = resolveFile(documentId);
        if(!file.delete())
            throw new FileNotFoundException();
    }

    @Override
    public String getDocumentType(String documentId) throws FileNotFoundException {
        File file = resolveFile(documentId);
        return getMimeType(file);
    }

    @Override
    public boolean isChildDocument(String parentDocumentId, String documentId) {
        return documentId.startsWith(parentDocumentId);
    }
}
