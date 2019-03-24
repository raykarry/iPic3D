clear all
close all
!rm *.png
!rm *.eps
clc
addpath(genpath('../../ipic3d_toolbox'))
folder_name = pwd;
folder_name = '/shared/gianni/emc2paper/PF-g3-ss0-qom64-run/PF-g3-ss0-qom64-damp-re80k'
namefile = 'PF4-Partcl';
namefile_field = 'PF4-Fields';

global Lx Ly Lz Xgsmrange Ygsmrange Zgsmrange 

Lx=45;
Ly=30;


qom=[-64,1];


i = 104000

    it=sprintf('%06.0f',i);
        
    fn=[folder_name,'/',namefile,'_',it,'.h5'];
    
    hinfo=hdf5info(fn);
    
    % uncomment this for a list of varibales available
    %hinfo.GroupHierarchy.Groups(:).Name
    
    is=0;
    
    q = hdf5read(fn,['/Step#0/q_' num2str(is) '/']);
    x = hdf5read(fn,['/Step#0/x_' num2str(is) '/']);
    y = hdf5read(fn,['/Step#0/y_' num2str(is) '/']);
    u = hdf5read(fn,['/Step#0/u_' num2str(is) '/']);
    v = hdf5read(fn,['/Step#0/v_' num2str(is) '/']);
    w = hdf5read(fn,['/Step#0/w_' num2str(is) '/']);
    
    
    fn_field=[folder_name,'/',namefile_field,'_',it,'.h5'];

    hinfo=hdf5info(fn_field);
    Nx= hinfo.GroupHierarchy.Groups.Groups.Groups(3).Datasets(1).Dims(1);
    Ny= hinfo.GroupHierarchy.Groups.Groups.Groups(3).Datasets(1).Dims(2);
    Nz= hinfo.GroupHierarchy.Groups.Groups.Groups(3).Datasets(1).Dims(3);
    old=0
    if(old)
    bx = hdf5read(fn_field,'/Step#0/Block/Btx/0/');
    by = hdf5read(fn_field,'/Step#0/Block/Bty/0/');
    bz = hdf5read(fn_field,'/Step#0/Block/Btz/0/');
    xc=linspace(0, Lx, Nx);
    yc=linspace(0, Ly, Ny);
    %AAz=vecpot(xc,yc,bx',by');AAz=AAz';

    iz=floor(Nz/2)
    AAz=vecpot(xc,yc,bx(:,:,iz),by(:,:,iz));AAz=AAz';
       else
    bx = hdf5read(fn_field,'/Step#0/Block/Bx/0/');
    by = hdf5read(fn_field,'/Step#0/Block/By/0/');
    bz = hdf5read(fn_field,'/Step#0/Block/Bz/0/');
    bx_ext = hdf5read(fn_field,'/Step#0/Block/Bx_ext/0/');
    by_ext = hdf5read(fn_field,'/Step#0/Block/By_ext/0/');
    bz_ext = hdf5read(fn_field,'/Step#0/Block/Bz_ext/0/');
    bx=bx+bx_ext;
    by=by+by_ext;
    bz=bz+bz_ext;
    
    bx=permute(squeeze(bx(:,:,round(Nz/2))),[2 1]);
    by=permute(squeeze(by(:,:,round(Nz/2))),[2 1]);
    bz=permute(squeeze(bz(:,:,round(Nz/2))),[2 1]);
    
    xc=linspace(0, Lx, Nx);
    yc=linspace(0, Ly, Ny);
    %AAz=vecpot(xc,yc,bx',by');AAz=AAz';

    iz=floor(Nz/2)
    xc=linspace(0, Lx, Nx);
    yc=linspace(0, Ly, Ny);
    ath=vecpot_cyl(xc,yc,bx,by);
    b=sqrt(bx.^2+by.^2+bz.^2);
    end
    %
    %   Extract subset
    %

    %
    %   Extract subset
    %
        radius = .2
        volgorde= 0
    for posx=0:radius:Lx
        posy=Ly/4
        volgorde=volgorde+1

    ii=abs(y-posy)<radius & abs(x-posx)<radius;
    sum(ii)
    qsub=q(ii);
    xsub=x(ii);
    ysub=y(ii);
    usub=u(ii); 
    vsub=v(ii);
    wsub=w(ii);
    Nsub=max(size(xsub))
    if Nsub>100
    %plot(usub,vsub,'.','MarkerSize',[1])
    %title(num2str(size(xsub)))
    if(is==0)
        vmax=.5;
    else
        vmax=.5/sqrt(64);
    end    
    vmin= -vmax;
    ndiv=100;
    vdf_sp=spaziofasi3D(usub,wsub,vsub,qsub,vmin,vmax,ndiv);
        %vdf_sp=vdf_sp./sum(vdf_sp(:));
        
    global color_choice symmetric_color titolo square labelT
      
   close all
    Nsm= 2
    square =1
    color_choice = 0
    symmetric_color = 0 
    labelT=''
    titolo=['Npart=' num2str(Nsub)];
    immagine_dir([vmin vmax],[vmin vmax],log10(1e-10+squeeze(sum(vdf_sp,2))), ...
             ['vdfXZ_' 'species_' num2str(is) '_' num2str(volgorde)], ...
             [0 5e-3]*0,Nsm,titolo,0,1,[1 3],'v_x/c','v_z/c','vdf');
    immagine_dir([vmin vmax],[vmin vmax],log10(1e-10+squeeze(sum(vdf_sp,3))), ...
             ['vdfXY_' 'species_' num2str(is) '_' num2str(volgorde)], ...
             [0 5e-3]*0,Nsm,titolo,0,1,[2 3],'v_x/c','v_y/c','vdf');
    %figure(1)
    subplot(1,3,3)
    contourf(xc,yc,log(ath.^2+1e-10))
    %pcolor(xc,yc,b)
    shading interp
    hold on
    plot(xsub,ysub,'w.')
    axis equal
    %set(gca,'xdir','reverse','TickDir','out')
    %xlim(gsmx([0 Lx]))
    %ylim(gsmy2z([0 Ly]))
    xlabel('x', 'fontsize',[14])
    ylabel('y', 'fontsize',[14])
    set(gca,'fontsize',[14])
    hold off
    print('-dpng',['combo' sprintf('%06.0f',volgorde)])

  

    end 
    
    end