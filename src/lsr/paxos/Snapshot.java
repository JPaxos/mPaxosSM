package lsr.paxos;

import static lsr.common.ProcessDescriptor.processDescriptor;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.Serializable;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import lsr.common.Reply;

/**
 * Structure - snapshot wrapped with all necessary additional data
 * 
 * The snapshot instanceId is the instanceId of the next instance to be
 * executed, so that one might also record a snapshot before any instance has
 * been decided (even if this has no real use, and enabling it is easy)
 * 
 * WARNING: this class is now extremely fragile because java copies memory all
 * around and we want to avoid it
 * 
 * @author JK
 */
public class Snapshot implements Serializable {
    private static final long serialVersionUID = -7961820683501513466L;

    // Replica part
    /** Id of next instance to be executed */
    private int nextIntanceId;
    /** Id of next instance to be executed */
    private long nextServiceSeqNo;

    /** Files of files representing the application state */
    private List<String> snapshotFiles;
    private Runnable freeSnapshotFiles;

    /** RequestId of last executed request for each client */
    private Reply[] lastReplyForClient;

    /**
     * Creates empty snapshot.
     */
    public Snapshot() {
    }

    /**
     * Reads previously recorded snapshot from input stream.
     * 
     * @param input - the input stream with serialized snapshot
     * @throws IOException if I/O error occurs
     */
    public Snapshot(DataInputStream input) throws IOException {

        // instance id
        nextIntanceId = input.readInt();

        // service seq no
        nextServiceSeqNo = input.readLong();

        // value
        int size = input.readInt();

        logger.trace("Unpacking snapshot files (snapshot value size: {})", size);
        readSnapFiles(input);

        // executed requests
        size = input.readInt();
        lastReplyForClient = new Reply[size];
        for (int i = 0; i < size; i++) {

            int replySize = input.readInt();
            byte[] reply = new byte[replySize];
            input.readFully(reply);

            lastReplyForClient[i] = new Reply(reply);
        }
    }

    public Snapshot(final ByteBuffer bb) {

        // instance id
        nextIntanceId = bb.getInt();

        // service seq no
        nextServiceSeqNo = bb.getLong();

        // value
        int size = bb.getInt();

        logger.trace("Unpacking snapshot files (snapshot value size: {})", size);
        readSnapFiles(new InputStream() {
            public int read() throws IOException {
                return bb.get();
            }

            public int read(byte[] b) throws IOException {
                int oldPos = bb.position();
                bb.get(b);
                return bb.position() - oldPos;
            }

            public int read(byte[] b, int off, int len) throws IOException {
                int oldPos = bb.position();
                bb.get(b, off, len);
                return bb.position() - oldPos;
            }
        });

        // executed requests
        size = bb.getInt();
        lastReplyForClient = new Reply[size];
        for (int i = 0; i < size; i++) {
            int replySize = bb.getInt();
            byte[] reply = new byte[replySize];
            bb.get(reply);

            lastReplyForClient[i] = new Reply(reply);
        }
    }

    protected void readSnapFiles(InputStream source) {
        long start = 0;
        if (logger.isDebugEnabled())
            start = System.nanoTime();

        byte[] buffer = new byte[Short.MAX_VALUE + 1];
        int length;
        snapshotFiles = new ArrayList<String>();

        try {
            if (processDescriptor.snapshotCompress) {

                ZipInputStream zis = new ZipInputStream(source);
                ZipEntry ze;
                while (null != (ze = zis.getNextEntry())) {
                    String path = ze.getName() + "~";
                    if (!ze.getName().startsWith("/"))
                        path = processDescriptor.nvmDirectory + "/" + path;

                    snapshotFiles.add(path);

                    FileOutputStream fos = new FileOutputStream(path);
                    long totalLength = 0;
                    while ((length = zis.read(buffer)) > 0) {
                        totalLength += length;
                        fos.write(buffer, 0, length);
                    }
                    fos.close();

                    logger.trace("Unpacked file {} ({} bytes, method {})", path, totalLength,
                            ze.getMethod());
                }
                zis.close();
            } else {
                DataInputStream dis = new DataInputStream(source);
                int snapFileCount = dis.readInt();
                for (int i = 0; i < snapFileCount; ++i) {
                    byte[] filenameBytes = new byte[dis.readInt()];
                    dis.readFully(filenameBytes);

                    String filename = new String(filenameBytes);
                    filename = filename + "~";
                    if (!filename.startsWith("/"))
                        filename = processDescriptor.nvmDirectory + "/" + filename;

                    snapshotFiles.add(filename);
                    FileOutputStream fos = new FileOutputStream(filename);

                    int remainingFileLength = dis.readInt();
                    do {
                        int bytesRead = dis.read(buffer, 0,
                                Math.min(remainingFileLength, buffer.length));
                        fos.write(buffer, 0, bytesRead);
                        remainingFileLength -= bytesRead;
                    } while (remainingFileLength > 0);

                    fos.close();
                }
            }
        } catch (IOException e) {
            throw new RuntimeException(e.getMessage(), e);
        }

        if (logger.isDebugEnabled()) {
            start = System.nanoTime() - start;
            logger.debug("ServiceProxy unpacked snapshot in {}s", start / 1.e9);
        }
    }

    /**
     * @return id of next instance to be executed
     */
    public int getNextInstanceId() {
        return nextIntanceId;
    }

    public void setNextInstanceId(int nextInstanceId) {
        this.nextIntanceId = nextInstanceId;
    }

    public long getNextServiceSeqNo() {
        return nextServiceSeqNo;
    }

    public void setNextServiceSeqNo(long nextServiceSeqNo) {
        this.nextServiceSeqNo = nextServiceSeqNo;
    }

    public Reply[] getLastReplyForClient() {
        return lastReplyForClient;
    }

    public void setLastReplyForClient(Reply[] lastReplyForClient) {
        this.lastReplyForClient = lastReplyForClient;
    }

    /**
     * Writes the snapshot at the end of given {@link ByteBuffer}
     */
    public void writeTo(ByteBuffer bb) {
        // auxiliary, for logging only; see upperByteSizeEstimate()
        int ubse = 0;
        if (logger.isDebugEnabled()) {
            for (String sf : snapshotFiles) {
                ubse += 66 + sf.length();
                long fileLen = (new File(sf).length());
                ubse += fileLen + ((fileLen + 0x3ffe) / 0x3fff) * 5;
                ubse += 24;
                ubse += 110 + sf.length();
            }
            ubse += 98;
        }

        // write when (logically) snapshot was created
        bb.putInt(nextIntanceId);
        bb.putLong(nextServiceSeqNo);

        // placeholder for the value (application-level snapshot) length
        int sizePosition = bb.position();
        bb.position(sizePosition + 4);

        // application-level snapshot that gets to compressed unforeseeable size
        produceValue(bb);

        // write the real length
        int valueSize = bb.position() - sizePosition - 4;
        bb.putInt(sizePosition, valueSize);
        logger.debug("SnapRealSize: {}, estimated: {}", valueSize, ubse);

        // write the reply map
        bb.putInt(lastReplyForClient.length);

        for (Reply r : lastReplyForClient) {
            bb.putInt(r.byteSize());
            bb.put(r.toByteArray());
        }
    }

    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null) {
            return false;
        }
        if (getClass() != obj.getClass()) {
            return false;
        }

        Snapshot other = (Snapshot) obj;
        return nextIntanceId == other.nextIntanceId && snapshotFiles.equals(other.snapshotFiles) &&
               lastReplyForClient.equals(other.lastReplyForClient);
    }

    public int hashCode() {
        int hash = 7;
        hash = 31 * hash + nextIntanceId;
        hash = (int) (31 * hash + nextServiceSeqNo);
        return hash;
    }

    public String toString() {
        return "Snapshot I:" + nextIntanceId + " SN:" + nextServiceSeqNo;
    }

    /**
     * @return detailed textual contents of the snapshot
     */
    public String dump() {
        StringBuilder sb = new StringBuilder();

        sb.append("Snapshot ").append(super.toString());
        sb.append("\n    Next instance ID: ").append(nextIntanceId);
        sb.append("\n    Next service seq no: ").append(nextServiceSeqNo);
        if (snapshotFiles != null) {
            sb.append("\n  File(s): ");
            for (String fn : snapshotFiles) {
                sb.append("\n    ").append(fn);
                File f = new File(fn);
                if (f.exists()) {
                    sb.append(' ').append(f.length()).append("bytes");
                } else {
                    sb.append(" DOES NOE EXIST");
                }
            }
        } else {
            sb.append("\n  No files.");
        }

        sb.append("\n  Last reply map has ").append(lastReplyForClient.length).append(" entries\n");
        /*-
        sb.append("\n  Last reply map (").append(lastReplyForClient.size()).append(" in total):\n");
        for (Reply reply : lastReplyForClient.values()) {
            sb.append(reply.toString()).append(", ");
        }
        sb.append("\n"); */
        return sb.toString();
    }

    public void setSnapshotFiles(List<String> fileList, Runnable freeFiles) {
        this.snapshotFiles = fileList;
        this.freeSnapshotFiles = freeFiles;
    }

    public List<String> getSnapshotFiles() {
        return snapshotFiles;
    }

    // java can zip only with stream API…
    private void produceValue(final ByteBuffer bb) {
        produceValue(new OutputStream() {
            public void write(int singleByte) throws IOException {
                bb.put((byte) singleByte);
            }

            public void write(byte[] data) throws IOException {
                bb.put(data);
            }

            public void write(byte[] data, int offset, int length) throws IOException {
                bb.put(data, offset, length);
            }
        });
    }

    private void produceValue(OutputStream baos) {
        long totalTime = 0;

        if (logger.isDebugEnabled()) {
            logger.trace("Turning snapshot files into memory byte buffer");
            totalTime = System.nanoTime();
        }

        long totalLength = 0;

        try {
            if (processDescriptor.snapshotCompress) {

                ZipOutputStream zip = new ZipOutputStream(baos);
                zip.setLevel(processDescriptor.snapshotCompressionLevel);

                for (String file : snapshotFiles) {
                    File f = new File(file);
                    FileInputStream fis;

                    if (logger.isTraceEnabled())
                        logger.trace("Adding file {} of size {}", file, f.length());

                    try {
                        fis = new FileInputStream(f);
                    } catch (FileNotFoundException e) {
                        throw new RuntimeException("File path provided by service is nonexistent",
                                e);
                    }

                    // if path is relative to nvmDirectory, then strip it
                    if (file.startsWith(processDescriptor.nvmDirectory)) {
                        file = file.substring(processDescriptor.nvmDirectory.length());
                        while (file.startsWith("/"))
                            file = file.substring(1);
                        // obvious directory traversal vulnerability is obvious
                    }
                    zip.putNextEntry(new ZipEntry(file));

                    byte[] buffer = new byte[Short.MAX_VALUE + 1];

                    int length;
                    while ((length = fis.read(buffer)) >= 0) {
                        totalLength += length;
                        zip.write(buffer, 0, length);
                    }

                    fis.close();
                }
                zip.close();
            } else {
                DataOutputStream dos = new DataOutputStream(baos);
                dos.writeInt(snapshotFiles.size());
                for (String file : snapshotFiles) {
                    File f = new File(file);

                    long fsize = f.length();
                    if (fsize > Integer.MAX_VALUE)
                        throw new RuntimeException("Snpahsot file " + file + " too big");

                    // TODO: if the files changes size, we're doomed. Fix it
                    // later with some javilish magic for this purpose - in C
                    // one would just open the file for reading and writing...
                    FileInputStream fis = new FileInputStream(f);

                    // if path is relative to nvmDirectory, then strip it
                    if (file.startsWith(processDescriptor.nvmDirectory)) {
                        file = file.substring(processDescriptor.nvmDirectory.length());
                        while (file.startsWith("/"))
                            file = file.substring(1);
                        // obvious directory traversal vulnerability is obvious
                    }
                    byte[] nameBytes = file.getBytes();
                    dos.writeInt(nameBytes.length);
                    dos.write(nameBytes);
                    dos.writeInt((int) fsize);

                    byte[] buffer;
                    buffer = new byte[Short.MAX_VALUE + 1];

                    int length;
                    while ((length = fis.read(buffer)) >= 0) {
                        dos.write(buffer, 0, length);
                    }
                    fis.close();
                }
                dos.flush();
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        freeSnapshotFiles.run();

        freeSnapshotFiles = null;
        snapshotFiles = null;

        if (logger.isDebugEnabled()) {
            totalTime = System.nanoTime() - totalTime;
            logger.debug(
                    "ServiceProxy made snapshot in {}s, uncompressed file lengths sum up to {}",
                    totalTime / 1.e9, totalLength);
        }
    }

    // Snapshot byte size is not known before compression, so we use upper bound
    // on uncompressed size to get an estimate how big the snapshot can be
    public int upperByteSizeEstimate() {
        long size = 4; // next instance ID
        size += 8; // next service seq no
        size += 4; // value length

        // value; see:
        // https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/util/zip/ZipOutputStream.java
        // https://tools.ietf.org/html/rfc1951
        for (String sf : snapshotFiles) {
            size += 66 + sf.length();
            long fileLen = (new File(sf).length());
            // that's fileLen + ceil(fileLen/deflate blocksize)*5
            size += fileLen + ((fileLen + Short.MAX_VALUE - 1) / Short.MAX_VALUE) * 5;
            size += 24;
            size += 110 + sf.length();
        }
        size += 98;

        // last replies
        size += 4;
        for (Reply reply : lastReplyForClient)
            size += 4 + reply.byteSize();

        if ((size + Integer.BYTES + 1 + 4 + 8 + 8) > Integer.MAX_VALUE)
            throw new RuntimeException(
                    "CatchUpSnapshot might exceed Integer.MAX_VALUE bytes, and JPaxos does not support that large messages");

        return (int) size;
    }

    private final static Logger logger = LoggerFactory.getLogger(Snapshot.class);
}
