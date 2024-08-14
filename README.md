İşte Lenovo Vantage for Linux için bir README dosyası taslağı:


# Lenovo Vantage for Linux

Lenovo Vantage for Linux, Lenovo dizüstü bilgisayarlar için GNOME masaüstü ortamında çalışan bir yardımcı programdır. Bu araç, donanım özelliklerinizi yönetmenizi ve izlemeyi sağlar.

## Özellikler

- **Batarya Koruma Modu**: Bataryanın ömrünü uzatmak için şarj sınırını belirleyin.
- **USB Her Zaman Açık**: Bilgisayar kapalıyken bile USB bağlantı noktalarını aktif tutun.
- **Fn Kilidi Yönetimi**: Fn tuşu kilidi durumunu değiştirin.

## Desteklenen Platformlar

Şu anda yalnızca **Fedora GNOME** ortamı desteklenmektedir.

## Bilinen Hatalar

- **Mikrofon Açma/Kapama**: Bu özellik şu anda çalışmamaktadır.
- **Touchpad Açma/Kapama**: Bu özellik şu anda çalışmamaktadır.

Bu hatalar gelecekteki sürümlerde düzeltilecektir.

## Gereksinimler

Bu aracı kullanmadan önce aşağıdaki kütüphaneleri kurmanız gerekmektedir:

```bash
sudo dnf install power-profiles-daemon
```

Ek olarak, GNOME masaüstü ortamının yüklü ve yapılandırılmış olması gereklidir.

## Kurulum

1. Depoyu klonlayın:

    ```bash
    git clone https://github.com/bykemalh/vantage.git
    cd vantage
    ```

2. Projeyi kurun:

    ```bash
    meson setup build
    meson compile -C build
    sudo meson install -C build
    ```

3. GNOME ortamında Lenovo Vantage'ı başlatın.

## Katkıda Bulunma

Katkıda bulunmak isterseniz, lütfen bir **pull request** gönderin veya bir **issue** açın. Her türlü geri bildirimi ve öneriyi memnuniyetle karşılıyoruz.

## Lisans

Bu proje **GPLv3** lisansı altında lisanslanmıştır. Daha fazla bilgi için `LICENSE` dosyasını inceleyebilirsiniz.

## İletişim

Geri bildirim veya sorularınız için [bykemalh@gmail.com](mailto:bykemalh@gmail.com) adresinden benimle iletişime geçebilirsiniz.
```

### Açıklamalar:
- **Özellikler** bölümünde uygulamanın sunduğu temel işlevler özetlenmiştir.
- **Bilinen Hatalar** bölümünde kullanıcıları bilgilendirmek için mevcut sorunlar listelenmiştir.
- **Gereksinimler** kısmında uygulamanın düzgün çalışabilmesi için gerekli olan yazılım ve kütüphaneler belirtilmiştir.
- **Kurulum** bölümünde uygulamanın nasıl kurulacağı adım adım açıklanmıştır.
- **Katkıda Bulunma** ve **Lisans** bölümleri, açık kaynak projeler için standart bölümlerdir ve projeye nasıl katkıda bulunulabileceğini açıklar.

Bu README dosyasını projenizin kök dizinine ekleyerek GitHub'da projeyi tanıtabilirsiniz.
